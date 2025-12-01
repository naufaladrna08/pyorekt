#include "server.h"
#include <git2.h>
#include <format>
#include "json.hpp"

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

void Server::createRepository(const httplib::Request& req, httplib::Response& res) {
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
    std::string sql = "INSERT INTO transaction (id, name, code, description, path) VALUES ($1, $2, $3, $4, $5);";

    std::cout << std::format("Creating repository: {} at {}", name, path) << std::endl;
    
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
