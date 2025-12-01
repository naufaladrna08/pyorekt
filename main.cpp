#include <functional>
#include <git2/global.h>
#include <iostream>
#include <format>
#include "httplib.h"
#include "dotenv.h"

#include <libpq-fe.h>
#include <git2.h>
#include "server.h"

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

  Server server(env, conn);
  
  svr.Post("/repository/create", [&](const httplib::Request& req, httplib::Response& res) {
    server.createRepository(req, res);
  });

  std::cout << std::format("Listening on {}:{}", host, port) << std::endl;
  svr.listen(host, port);
  PQfinish(conn);
  return 0;
}