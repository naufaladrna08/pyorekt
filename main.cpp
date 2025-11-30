#include <git2/global.h>
#include <iostream>
#include <format>
#include "httplib.h"
#include "dotenv.h"

#include <libpq-fe.h>
#include "json.hpp"
#include <git2.h>

#define UUID_SYSTEM_GENERATOR
#include "uuid.h"

std::string create_code(const std::string& name) {
  std::string code;
  for (char c : name) {
    if (isalnum(c)) {
      code += tolower(c);
    } else if (c == ' ' || c == '-' || c == '_') {
      code += '-';
    }
  }
  return code;
}

void create_repository_action(const DotEnv& env, PGconn* conn, const httplib::Request& req, httplib::Response& res) {
  std::string git_root = env.get("GIT_ROOT");
  nlohmann::json response_json;

  // Start transaction 
  PGresult* res_db = PQexec(conn, "BEGIN");
  if (PQresultStatus(res_db) != PGRES_COMMAND_OK) {
    res.status = 500;
    response_json["error"] = std::format("Failed to start transaction: {}", PQerrorMessage(conn));
    std::cerr << std::format("Database error: {}", PQerrorMessage(conn)) << std::endl;
    res.set_content(response_json.dump(), "application/json");
    PQclear(res_db);
    return;
  }
  PQclear(res_db);

  try {
    auto req_json = nlohmann::json::parse(req.body);
    std::random_device rd;
    std::mt19937 gen32{rd()};

    uuids::uuid_random_generator gen{gen32};
    uuids::uuid id = gen();
    std::string uuid_str = uuids::to_string(id);
    std::string name = req_json.at("name").get<std::string>();
    std::string code = create_code(name);
    std::string path = std::format("{}/{}-{}.git", git_root, code, uuid_str);
    std::string description = req_json.value("description", "");
    std::string sql = "INSERT INTO repositories (id, name, code, description, path) VALUES ($1, $2, $3, $4, $5);";
    
    /* Insert into database */
    const char* paramValues[5] = { uuid_str.c_str(), name.c_str(), code.c_str(), description.c_str(), path.c_str() };
    PGresult* res_db = PQexecParams(conn, sql.c_str(), 5, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res_db) != PGRES_COMMAND_OK) {
      res.status = 500;
      response_json["error"] = std::format("Failed to insert repository into database: {}", PQerrorMessage(conn));
      std::cerr << std::format("Database error: {}", PQerrorMessage(conn)) << std::endl;
      res.set_content(response_json.dump(), "application/json");
      PQclear(res_db);
      return;
    }
    
    PQclear(res_db);
  
    /* Create git repository */
    git_libgit2_init();

    git_repository* repo = nullptr;
    int error = git_repository_init(&repo, path.c_str(), true);

    if (error < 0) {
      const git_error* e = git_error_last();
      res.status = 500;
      response_json["error"] = std::format("Failed to create repository: {}", e->message);
      std::cerr << std::format("Git error: {}", e->message) << std::endl;
      res.set_content(response_json.dump(), "application/json");
      return;
    }

    git_repository_free(repo);

    /* Commit transaction */
    res_db = PQexec(conn, "COMMIT");
    if (PQresultStatus(res_db) != PGRES_COMMAND_OK) {
      res.status = 500;
      response_json["error"] = std::format("Failed to commit transaction: {}", PQerrorMessage(conn));
      std::cerr << std::format("Database error: {}", PQerrorMessage(conn)) << std::endl;
      res.set_content(response_json.dump(), "application/json");
      PQclear(res_db);
      return;
    }

    res.status = 201;
    response_json["message"] = "Repository created successfully.";
    response_json["repository_path"] = path;
    res.set_content(response_json.dump(), "application/json");
    git_libgit2_shutdown();
  } catch (const std::exception& e) {
    /* Rollback transaction */
    PGresult* res_db = PQexec(conn, "ROLLBACK");
    PQclear(res_db);

    res.status = 400;
    response_json["error"] = std::format("Invalid request: {}", e.what());
    std::cerr << std::format("Request error: {}", e.what()) << std::endl;
    res.set_content(response_json.dump(), "application/json");
  }
}

int main(int argc, char** argv) {
  DotEnv env;
  if (env.load(".env") != 0) {
    std::cerr << "Failed to load .env file" << std::endl;
    return 1;
  }

  std::string git_root = env.get("GIT_ROOT");
  int port = std::stoi(env.get("PORT"));
  std::string host = env.get("HOST");

  /* GIT_ROOT should not be empty, although we didn't check for the availability 
   * of the directory. Since this is a critical configuration, we exit if it's 
   * not set. */
  if (git_root.empty()) {
    std::cerr << "GIT_ROOT is not set in environment variables." << std::endl;
    return 1;
  }

  httplib::Server svr;
  std::string db_conninfo = env.get("DB_CONNINFO");
  PGconn* conn = PQconnectdb(db_conninfo.c_str());

  if (PQstatus(conn) != CONNECTION_OK) {
    std::cerr << std::format("Connection to database failed: {}", PQerrorMessage(conn)) << std::endl;
    PQfinish(conn);
    return 1;
  }

  svr.Post("/create-repository", [&](const httplib::Request& req, httplib::Response& res) {
    create_repository_action(env, conn, req, res);
  });

  std::cout << std::format("Listening on {}:{}", host, port) << std::endl;
  svr.listen(host, port);
  PQfinish(conn);
  return 0;
}