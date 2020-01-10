#include "client_http.hpp"
#include "server_http.hpp"

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

const std::string RESOURCE_PATH = "web";
const std::string settingOriginFile = RESOURCE_PATH + "/index_origin.html";
const std::string settingFile = RESOURCE_PATH + "/index.html";
const std::string settingWithMicRowFile = RESOURCE_PATH + "/indexsetup.html";
const std::string updatePasswordFile = RESOURCE_PATH + "/updatepassword.html";
const std::string loginFile = RESOURCE_PATH + "/login.html";
const std::string loginFailedFile = RESOURCE_PATH + "/loginfailed.html";
const std::string rowCamFile = RESOURCE_PATH + "/rowCam.html";

#include <vector>
#include <map>

vector<string> split_string(string s, string delim)
{
  vector<string> stringList;
  size_t pos;

  while((pos = s.find(delim)) != string::npos)
  {
    auto token = s.substr(0, pos);
    s.erase(0, pos + delim.length());
    stringList.push_back(token);
  }

  if(s.length() > 0)
  {
    stringList.push_back(s);
  }

  return stringList;
}

map<string, string> parse_request_content(string request)
{
  auto contents = split_string(request, "&");
  map<string, string> request_content;

  for(auto content: contents)
  {
    auto tokens = split_string(content, "=");
    if(tokens.size() == 2)
    {
      request_content[tokens[0]] = tokens[1];
    }
  }

  return request_content;
}

const std::string get_content_file(std::string file_name)
{
  std::ifstream t(file_name);
  std::string str((std::istreambuf_iterator<char>(t)),
    std::istreambuf_iterator<char>());

  return str;
}

void write_content_to_file(std::string file_name, std::string content)
{
  std::ofstream outfile(file_name);

	if (outfile.is_open())
	{
		outfile << content ;
	}
	else
	{
		cout << "Open " << file_name << " for writing failed" << endl;
	}
	outfile.close();
}

std::string replace_string(std::string subject, const std::string& search,
	const std::string& replace) 
{
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
      subject.replace(pos, search.length(), replace);
      pos += replace.length();
  }
  return subject;
}

class MicInfo
{
public:
  std::string username;
  std::string password;
  std::string ip;
  std::string macaddress;
  int port;
  std::string micname;
  int micindex;

  bool operator== (MicInfo const& _info)
  {
    return this->macaddress == _info.macaddress;
  }

  MicInfo& operator =(const MicInfo& _info)
  { 
    this->username = _info.username;
    this->password = _info.password;
    this->ip = _info.ip;
    this->macaddress = _info.macaddress;
    this->port = _info.port;
    this->micname = _info.micname;
    this->micindex = _info.micindex;
  }
};

class MQTTBrokerInfo
{
private:

public:
  MQTTBrokerInfo() {}
  ~MQTTBrokerInfo() {}

  std::string ip;
  std::string username;
  std::string password;
  std::string status;
};


class HVSConfig
{
private:
  /* data */
  vector<MicInfo> mic_list;
  MQTTBrokerInfo broker_info;
public:
  HVSConfig(/* args */){}
  ~HVSConfig(){}

  const vector<MicInfo> get_mic_list() {
    return this->mic_list;
  } 

  MQTTBrokerInfo& get_broker_info()
  {
    return this->broker_info;
  }

  void add_mic(MicInfo _info)
  {
    // check mic exist
    for(auto & mic: mic_list)
    {
      if(mic == _info)
      {
        // edit mic infomation
        mic = _info;
        return;
      }
    }
    mic_list.push_back(_info);
  }

  void rm_mic(MicInfo _info)
  {
    for(auto it_mic=mic_list.begin(); it_mic != mic_list.end(); it_mic++)
    {
      if(it_mic->micindex == _info.micindex)
      {
        mic_list.erase(it_mic);
        break;
      }
    }
  }

  void rm_mic(int _mic_index)
  {
    mic_list.erase(mic_list.begin() + _mic_index);
  }
};

static HVSConfig g_hvs_config;

std::string display_miclist()
{
  std::string outString, line;

  std::ifstream in(settingFile);
  if(in.fail())
  {
    cout << "Open " << settingFile << " for reading failed" << endl;
    return "";
  }

  while(std::getline(in, line))
  {
    outString += line + "\n";
    if(line.find("<tbody>") != std::string::npos)
    {
      auto mic_list = g_hvs_config.get_mic_list();

      for(size_t i=0; i<mic_list.size(); i++)
      {
        auto mic = mic_list.at(i);

        auto rowCamStr = get_content_file(rowCamFile);

        rowCamStr = replace_string(rowCamStr, "{index}", std::to_string(i));
        rowCamStr = replace_string(rowCamStr, "MICNAME", mic.micname);
				rowCamStr = replace_string(rowCamStr, "MICIP", mic.ip) ;
				rowCamStr = replace_string(rowCamStr, "MICPORT", std::to_string(mic.port));
				rowCamStr = replace_string(rowCamStr, "MICUSER", mic.username);
				rowCamStr = replace_string(rowCamStr, "MICPASS", mic.password);
				rowCamStr = replace_string(rowCamStr, "MICMAC", mic.macaddress);
				rowCamStr = replace_string(rowCamStr, "MICINDEX", std::to_string(i));
        outString += rowCamStr + "\n";
      }

      // ignore old data until find </tbody>
      while (std::getline(in, line))
      {
        if(line.find("</tbody>") != std::string::npos)
        {
          outString += line + "\n";
          break;
        }
      }
    }

    if (line.find("BROKER_IP") != std::string::npos)
    {
      
      size_t pos1 = line.find("\"");
      
      size_t pos2 = line.find("\"", pos1 + 1);
      
      if (pos1 != 0 && pos2 != 0)
      {
        int len = pos2 - pos1 - 1;

        line.replace(pos1 + 1, len, g_hvs_config.get_broker_info().ip);
      }
    }

    if (line.find("BROKER_USER") != std::string::npos)
    {
      
      size_t pos1 = line.find("\"");
      
      size_t pos2 = line.find("\"", pos1 + 1);
      
      if (pos1 != 0 && pos2 != 0)
      {
        int len = pos2 - pos1 - 1;

        line.replace(pos1 + 1, len, g_hvs_config.get_broker_info().username);
      }
      
    }

    if (line.find("BROKER_PASSWORD") != std::string::npos)
    {
      
      size_t pos1 = line.find("\"");
      
      size_t pos2 = line.find("\"", pos1 + 1);
      
      if (pos1 != 0 && pos2 != 0)
      {
        int len = pos2 - pos1 - 1;

        line.replace(pos1 + 1, len, g_hvs_config.get_broker_info().password);
      } 
    }
  }

  in.close();

  write_content_to_file(settingFile, outString);

  return outString;
}


int main() {

  // reset index.html
  write_content_to_file(settingFile, get_content_file(settingOriginFile));

  // HTTP-server at port 8080 using 1 thread
  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpServer server;
  server.config.port = 8080;

  // Add resources using path-regex and method-string, and an anonymous function
  // POST-example for the path /string, responds the posted string
  // server.resource["^/string$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
  //   // Retrieve string:
  //   auto content = request->content.string();
  //   // request->content.string() is a convenience function for:
  //   // stringstream ss;
  //   // ss << request->content.rdbuf();
  //   // auto content=ss.str();

  //   *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
  //             << content;


  //   // Alternatively, use one of the convenience functions, for instance:
  //   // response->write(content);
  // };

  // POST-example for the path /json, responds firstName+" "+lastName from the posted json
  // Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
  // Example posted json:
  // {
  //   "firstName": "John",
  //   "lastName": "Smith",
  //   "age": 25
  // }
  // server.resource["^/json$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
  //   try {
  //     ptree pt;
  //     read_json(request->content, pt);

  //     auto name = pt.get<string>("firstName") + " " + pt.get<string>("lastName");

  //     *response << "HTTP/1.1 200 OK\r\n"
  //               << "Content-Length: " << name.length() << "\r\n\r\n"
  //               << name;
  //   }
  //   catch(const exception &e) {
  //     *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n"
  //               << e.what();
  //   }


    // Alternatively, using a convenience function:
    // try {
    //     ptree pt;
    //     read_json(request->content, pt);

    //     auto name=pt.get<string>("firstName")+" "+pt.get<string>("lastName");
    //     response->write(name);
    // }
    // catch(const exception &e) {
    //     response->write(SimpleWeb::StatusCode::client_error_bad_request, e.what());
    // }
  // };

  // GET-example for the path /info
  // Responds with request-information
  // server.resource["^/info$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
  //   stringstream stream;
  //   stream << "<h1>Request from " << request->remote_endpoint_address() << ":" << request->remote_endpoint_port() << "</h1>";

  //   stream << request->method << " " << request->path << " HTTP/" << request->http_version;

  //   stream << "<h2>Query Fields</h2>";
  //   auto query_fields = request->parse_query_string();
  //   for(auto &field : query_fields)
  //     stream << field.first << ": " << field.second << "<br>";

  //   stream << "<h2>Header Fields</h2>";
  //   for(auto &field : request->header)
  //     stream << field.first << ": " << field.second << "<br>";

  //   response->write(stream);
  // };

  // GET-example for the path /match/[number], responds with the matched string in path (number)
  // For instance a request GET /match/123 will receive: 123
  // server.resource["^/match/([0-9]+)$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
  //   response->write(request->path_match[1]);
  // };

  // // GET-example simulating heavy work in a separate thread
  // server.resource["^/work$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
  //   thread work_thread([response] {
  //     this_thread::sleep_for(chrono::seconds(5));
  //     response->write("Work done");
  //   });
  //   work_thread.detach();
  // };

  server.resource["^/update$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
  
    auto req = request->content.string();
    
    auto req_content = parse_request_content(req);

    MicInfo mic_info;
    mic_info.ip = req_content["mic-ip"];
    mic_info.macaddress = req_content["mic-macaddr"];
    mic_info.micname = req_content["mic-name"];
    mic_info.port = std::atoi(req_content["mic-port"].c_str());
    mic_info.username = req_content["mic-username"];
    mic_info.password = req_content["mic-password"];

    g_hvs_config.add_mic(mic_info);

    string res = display_miclist();
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << res.length() << "\r\n\r\n"
              << res;
  };

  server.resource["^/delete"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    cout << "delete " << request->content.string() << endl;

    auto req = request->content.string();
    auto req_content = parse_request_content(req);
    int mic_index = std::atoi(req_content["mic-index"].c_str());

    g_hvs_config.rm_mic(mic_index);

    string res = display_miclist();
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << res.length() << "\r\n\r\n"
              << res;
  };

  server.resource["^/setting_form_handler$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto req = request->content.string();
    auto req_content = parse_request_content(req);

    auto & broker_info = g_hvs_config.get_broker_info();
    broker_info.ip = req_content["BROKER_IP"];
    broker_info.password = req_content["BROKER_PASSWORD"];
    
    cout << g_hvs_config.get_broker_info().ip << endl;


    // auto stringList = splitString(request->content.string(), "&");
    // for(auto s: stringList)
    // {
    //   cout << s << endl;
    // }

    string res = display_miclist();
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << res.length() << "\r\n\r\n"
              << res;
  };

  server.resource["^/login_form_handler$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto req = request->content.string();
    auto req_content = parse_request_content(req);

    cout << req << "\n";


    // auto stringList = splitString(request->content.string(), "&");
    // for(auto s: stringList)
    // {
    //   cout << s << endl;
    // }

    string res = display_miclist();
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << res.length() << "\r\n\r\n"
              << res;
  };

  // Default GET-example. If no other matches, this anonymous function will be called.
  // Will respond with content in the web/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("web");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "login.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      // Uncomment the following line to enable Cache-Control
      // header.emplace("Cache-Control", "max-age=86400");

#ifdef HAVE_OPENSSL
//    Uncomment the following lines to enable ETag
//    {
//      ifstream ifs(path.string(), ifstream::in | ios::binary);
//      if(ifs) {
//        auto hash = SimpleWeb::Crypto::to_hex_string(SimpleWeb::Crypto::md5(ifs));
//        header.emplace("ETag", "\"" + hash + "\"");
//        auto it = request->header.find("If-None-Match");
//        if(it != request->header.end()) {
//          if(!it->second.empty() && it->second.compare(1, hash.size(), hash) == 0) {
//            response->write(SimpleWeb::StatusCode::redirection_not_modified, header);
//            return;
//          }
//        }
//      }
//      else
//        throw invalid_argument("could not read file");
//    }
#endif

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
    }
  };

  server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  thread server_thread([&server]() {
    // Start server
    server.start();
  });

  // Wait for server to start so that the client can connect
  this_thread::sleep_for(chrono::seconds(1));

  // Client examples
  // HttpClient client("localhost:8080");

  // string json_string = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";

  // // Synchronous request examples
  // try {
  //   auto r1 = client.request("GET", "/match/123");
  //   cout << r1->content.rdbuf() << endl; // Alternatively, use the convenience function r1->content.string()

  //   auto r2 = client.request("POST", "/string", json_string);
  //   cout << r2->content.rdbuf() << endl;
  // }
  // catch(const SimpleWeb::system_error &e) {
  //   cerr << "Client request error: " << e.what() << endl;
  // }

  // // Asynchronous request example
  // client.request("POST", "/json", json_string, [](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
  //   if(!ec)
  //     cout << response->content.rdbuf() << endl;
  // });
  // client.io_service->run();

  server_thread.join();
}
