/* 
 * Single-header C++ library for loading environment variables from 
 * a .env file into the program's memory to facilitate configuration 
 * management. The library uses RAII principles to ensure proper
 * resource management.
 */

#ifndef DOTENV_H
#define DOTENV_H

#include <unordered_map>
#include <string>
#include <cstdio>

class DotEnv {
  public:
    DotEnv() = default;
    ~DotEnv() = default;

    inline int load(const std::string& filepath) {
      FILE* file = fopen(filepath.c_str(), "r");
      if (!file) {
        return -1; // Return an error code if the file cannot be opened
      }
      
      char line[256];
      while (fgets(line, sizeof(line), file)) {
        std::string str_line(line);
        
        // Skip empty lines and comments
        if (str_line.empty() || str_line[0] == '#') {
          continue;
        }
        
        size_t eq_pos = str_line.find('=');
        if (eq_pos != std::string::npos) {
          std::string key = str_line.substr(0, eq_pos);
          std::string value = str_line.substr(eq_pos + 1);
          
          // Trim trailing whitespace (including newlines)
          size_t end = value.find_last_not_of(" \t\n\r");
          if (end != std::string::npos) {
            value = value.substr(0, end + 1);
          } else {
            value.clear();
          }

          // Remove surrounding quotes if present
          if ((value.front() == '"' && value.back() == '"') ||
              (value.front() == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
          }
          
          // Trim key as well
          size_t key_end = key.find_last_not_of(" \t");
          if (key_end != std::string::npos) {
            key = key.substr(0, key_end + 1);
          }
          
          m_envVars[key] = value;
        }
      }

      fclose(file);
      return 0; // Return 0 on success
    }

    inline std::string get(const std::string& key, const std::string& defaultValue = "") const {
      auto it = m_envVars.find(key);
      if (it != m_envVars.end()) {
        return it->second;
      }
      return defaultValue; // Return default value if key not found
    }

  private:
    std::unordered_map<std::string, std::string> m_envVars;
};

#endif