#ifndef SERVER_H
#define SERVER_H

#include "httplib.h"
#include "dotenv.h"
#include <libpq-fe.h>

struct Server {
  DotEnv env;
  PGconn* conn;

  Server(const DotEnv& dotenv, PGconn* connection) : env(dotenv), conn(connection) {}

  void createRepository(const httplib::Request& req, httplib::Response& res);
  void listRepositories(const httplib::Request& req, httplib::Response& res);
};

#endif