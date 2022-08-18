/*
 * Made by Alexander Yefimenko. All right reserved.
 *
 */

#include "config.h"
#include "types.h"
#include "technic_info.h"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>

#include <parser2db.h>

#include <chrono>
#include <thread>

#include <curl/curl.h>
#include <expat.h>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

using namespace std;

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace src = boost::log::sources;
namespace attr = boost::log::attributes;
namespace sinks = boost::log::sinks;
namespace bpo = boost::program_options;

typedef sinks::synchronous_sink<sinks::text_file_backend> sink_t;
static boost::shared_ptr<sink_t> fssink;

static Config config;

static Technic_info ti;
static std::vector<Technic_info> tiv;
static std::map<std::string , int> vecmidmap;
static std::map<std::string, Technic_info> vec_aux_info_map;

//------------------------------------------------------------------------------
static
void
setup_logging(const std::string & log_file_path,
              int log_level)
{
  if (log_level == 6)
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
  else if (log_level == 5)
     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);
  else if (log_level == 4)
     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
  else if (log_level == 3)
     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  else if (log_level == 2)
     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::error);
  else if (log_level == 1)
     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::fatal);
  else
     logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::fatal);

  auto fmttimestamp = expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f");
  auto fmttprocessid = expr::attr<logging::process_id>("ProcessID");
  auto fmtthreadid = expr::attr<attr::current_thread_id::value_type>("ThreadID");
  auto fmtseverity = expr::attr<logging::trivial::severity_level>("Severity");
  auto fmtscope = expr::attr<attr::named_scope::value_type>("Scope");

  logging::formatter logfmt = expr::format("%1%|%2%|%3%|%4%\n\t%5%%6%")
                                            % fmttimestamp % fmttprocessid
                                            % fmtthreadid % fmtseverity
                                            % fmtscope % expr::smessage;

  if (config.consolelog()) {
    auto consolesink = logging::add_console_log(std::clog);
    consolesink->set_formatter(logfmt);
  }

  fssink = logging::add_file_log(keywords::file_name = log_file_path,
                                 keywords::open_mode = std::ios_base::app);

  fssink->set_formatter(logfmt);
  fssink->locked_backend()->auto_flush(true);

  logging::add_common_attributes();
  logging::core::get()->add_global_attribute("Scope", attr::named_scope());
}

//------------------------------------------------------------------------------
static void startElement(void *userData,
                         const XML_Char *name,
                         const XML_Char **atts)
{
  try {
    if ((strcmp(name, "Technic") == 0)) {
      for (int i=0; atts[i]; i+=2)  {
        if (strcmp(atts[i], "vehicle_export_code") == 0) {
          if (strlen(atts[i+1]))
            ti.vec = atts[i+1];
        }
        else if (strcmp(atts[i], "message_id") == 0) {
          if (strlen(atts[i+1])) {
            ti.mid = atts[i+1];
            if (ti.mid.compare("None") != 0) {
              try {
                ti.mid_int = stoi(atts[i+1]);
              } catch (std::exception & ex) {}
            }
          }
        } else if (strcmp(atts[i], "datetime") == 0) {
          ti.dt = atts[i+1];
        }
      }
    } else if ((strcmp(name, "Parameter") == 0)) {
      std::string last_name ="";
      for (int i=0; atts[i]; i+=2)  {
        if (strcmp(atts[i], "name") == 0) {
            last_name = atts[i+1];
        }
        else if (strcmp(atts[i], "value") == 0) {
          if (last_name.compare("latitude") == 0)
            ti.lat = atts[i+1];
          else if (last_name.compare("longitude") == 0)
            ti.lon = atts[i+1];
          else if (last_name.compare("speed") == 0)
            ti.speed = atts[i+1];
          else if (last_name.compare("fuel") == 0)
            ti.fuel = atts[i+1];
        }
      }
    }
  } catch (std::exception & ex) {
    BOOST_LOG_TRIVIAL(warning) << "got std::exception in phase startElem, reason: " << ex.what();
    ti.reset_data();
  }
}

//------------------------------------------------------------------------------
static void endElement(void *userData,
                       const XML_Char *name)
{
  if ((strcmp(name, "Technic") == 0)) {
    if (!ti.vec.empty() &&
        !ti.dt.empty() &&
        !ti.lat.empty() &&
        !ti.lon.empty()) {
      tiv.push_back(ti);
    }

    ti.reset_data();
  }
//  printf("%5lu   %10lu   %s\n", state->depth, state->characters.size, name);
}

//------------------------------------------------------------------------------
static void startElement2(void *userData,
                         const XML_Char *name,
                         const XML_Char **atts)
{
  try {
    if ((strcmp(name, "Technic") == 0)) {
      for (int i=0; atts[i]; i+=2)  {
        if (strcmp(atts[i], "export_code") == 0) {
          if (strlen(atts[i+1]))
            ti.vec = atts[i+1];
        } else if (strcmp(atts[i], "datetime") == 0) {
          ti.dt = atts[i+1];
        }
      }
    } else if ((strcmp(name, "Parameter") == 0)) {
      std::string last_name ="";
      for (int i=0; atts[i]; i+=2)  {
        if (strcmp(atts[i], "name") == 0) {
            last_name = atts[i+1];
        }
        else if (strcmp(atts[i], "value") == 0) {
          if (last_name.compare("latitude") == 0)
            ti.lat = atts[i+1];
          else if (last_name.compare("longitude") == 0)
            ti.lon = atts[i+1];
          else if (last_name.compare("speed") == 0)
            ti.speed = atts[i+1];
          else if (last_name.compare("height") == 0)
            ti.height = atts[i+1];
          else if (last_name.compare("course") == 0)
            ti.course = atts[i+1];
        }
      }
    } else if ((strcmp(name, "AnalyticEntity") == 0)) {
      std::string last_name, last_id;

      for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "id") == 0) {
          last_id = atts[i + 1];
        } else if (strcmp(atts[i], "name") == 0) {
            last_name = atts[i + 1];
        } else if (strcmp(atts[i], "value") == 0) {
          if (!last_id.compare("741")
              || !last_id.compare("742")
              || !last_id.compare("743")
              || !last_id.compare("744")
              || !last_id.compare("41")
              || !last_id.compare("202")
              )
          {
            if (std::strlen(atts[i + 1]) != 0) {
              ti.analytic_entity[last_id] = atts[i + 1];
//              vec_aux_info_map[ti.vec] = ti;
            }
          }
        }
      }
    }
  } catch (std::exception & ex) {
    BOOST_LOG_TRIVIAL(warning) << "got std::exception in phase startElem, reason: " << ex.what();
    ti.reset_data();
  }
}

//------------------------------------------------------------------------------
static void endElement2(void *userData,
                       const XML_Char *name)
{
  if ((strcmp(name, "Technic") == 0)) {
    if (!ti.vec.empty() &&
        !ti.dt.empty() &&
        !ti.lat.empty() &&
        !ti.lon.empty()) {
      tiv.push_back(ti);
    }

    ti.reset_data();
  }
//  printf("%5lu   %10lu   %s\n", state->depth, state->characters.size, name);
}

//------------------------------------------------------------------------------
static size_t parseStreamCallback(void *contents,
                                  size_t length,
                                  size_t nmemb,
                                  void *userp)
{
  XML_Parser parser = (XML_Parser) userp;
  size_t real_size = length * nmemb;

  // Only parse if we are not already in a failure state.
  if(XML_Parse(parser, (const char *)contents, real_size, 0) == 0) {
    auto error_code = XML_GetErrorCode(parser);

    BOOST_LOG_TRIVIAL(error) << "Parsing response buffer of length: "
                             <<  real_size
                             << " with error code: " << error_code
                             << "(" << XML_ErrorString(error_code) << ")"
                             << " failed";
  }

  return real_size;
}

//------------------------------------------------------------------------------
static void
process_data(std::vector<Technic_info> & tiv)
{
  for (auto i : tiv) {
//    if (vec_aux_info_map.contains(i.vec)) {
//      BOOST_LOG_TRIVIAL(trace) << "matching id: " << i.vec << " with vec_aux_info_map, dump id data:\n"
//         << i << "\n" << "dump map data:\n" << vec_aux_info_map[i.vec] << "\n";

//      i.dt = vec_aux_info_map[i.vec].dt;
//      i.lon = vec_aux_info_map[i.vec].lat;
//      i.lon = vec_aux_info_map[i.vec].lon;
//      i.wheel_preasure = vec_aux_info_map[i.vec].wheel_preasure;
//      i.height = vec_aux_info_map[i.vec].height;
//      i.course = vec_aux_info_map[i.vec].course;
//    }
    if (i.mid.empty()) {
      i.store_to_db();
    }
    else if (i.mid.compare("None") == 0) {
      i.store_to_db();
    }
    else if (vecmidmap[i.vec] != i.mid_int) {
      i.store_to_db();
      vecmidmap[i.vec] = i.mid_int;
    }
  }
}

//------------------------------------------------------------------------------
void handler1(const boost::system::error_code& error,
              boost::asio::deadline_timer* t)
{
  BOOST_LOG_NAMED_SCOPE("[handler1] ")

  try {
    if (!error) {
      CURL *curl;
      CURLcode res;

      XML_Parser parser,parser2;

      // Initialize a namespace-aware parser.
      parser = XML_ParserCreateNS(nullptr, '\0');
      XML_SetElementHandler(parser, startElement, endElement);

      // Initialize a namespace-aware parser.
      parser2 = XML_ParserCreateNS(nullptr, '\0');
      XML_SetElementHandler(parser2, startElement2, endElement2);

      curl = curl_easy_init();
      if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, config.vurl.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_USERNAME, config.vuser.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, config.vpwd.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, parseStreamCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)parser);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
          BOOST_LOG_TRIVIAL(error) << "curl_easy_perform() failed: " << curl_easy_strerror(res);
        }
        else {
          // Expat requires one final call to finalize parsing.
          if(XML_Parse(parser, nullptr, 0, 1) == 0) {
            int error_code = XML_GetErrorCode(parser);
            BOOST_LOG_TRIVIAL(error) << "Finalizing parsing failed with error code " << error_code << "(" << XML_ErrorString(XML_Error(error_code)) << ")";
          }
          else {
            if (!config.vurl2.empty()) {
              BOOST_LOG_TRIVIAL(trace) << "got  " << tiv.size() << " records after url1\n";
              curl_easy_setopt(curl, CURLOPT_URL, config.vurl2.c_str());
              curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)parser2);

              res = curl_easy_perform(curl);

              if (res != CURLE_OK) {
                BOOST_LOG_TRIVIAL(error) << "curl_easy_perform() failed: " << curl_easy_strerror(res);
              } else {
                // Expat requires one final call to finalize parsing.
                if (XML_Parse(parser2, nullptr, 0, 1) == 0) {
                  int error_code = XML_GetErrorCode(parser2);
                  BOOST_LOG_TRIVIAL(error) << "Finalizing parsing failed with error code " << error_code << "(" << XML_ErrorString(XML_Error(error_code)) << ")";
                } else {
                  BOOST_LOG_TRIVIAL(trace) << "got  " << tiv.size() << " records after url2\n";
                }
              }
            }
          }
        }

        // Clean up
        XML_ParserFree(parser);
        curl_easy_cleanup(curl);

        process_data(tiv);
        tiv.clear();
      }
    } else {
      BOOST_LOG_TRIVIAL(error) << "error in position report handler: " << error.message();
    }
  } catch(std::exception & ex) {
      BOOST_LOG_TRIVIAL(error) << "got std::exception, reason= " << ex.what();
  }

  t->expires_at(t->expires_at() + boost::posix_time::seconds(config.position_poll_timeout));
  t->async_wait(boost::bind(handler1, boost::asio::placeholders::error, t));
}

//------------------------------------------------------------------------------
static
int
setup_cmd_options(int argc, char *argv[]) {
  const std::string version{"VIST parser v0.1"};

  bpo::options_description usage("Usage");
  usage.add_options()
      ("version,v", "print version string")
      ("help", "produce help message")
      ("lfpath,l", bpo::value<std::string>(), "path to log file ")
      ("dbhost", bpo::value<std::string>(), "db host")
      ("dbport", bpo::value<int>(), "db port")
      ("dbname", bpo::value<std::string>(), "db name")
      ("dblogin", bpo::value<std::string>(), "db user")
      ("dbpwd", bpo::value<std::string>(), "db user pwd")
      ("vurl", bpo::value<std::string>(), "vurl")
      ("vurl2", bpo::value<std::string>(), "vurl2")
      ("vuser", bpo::value<std::string>(), "vuser")
      ("vpwd", bpo::value<std::string>(), "vpwd")
      ("consolelog", "skip logging to console")
      ("pospolltimeout,ppt", bpo::value<int>(), "position poll timeout")
      ("log-level", bpo::value<int>(), "logging level (1-6) more level, more details");

  bpo::variables_map vm;
  try {
    bpo::store(bpo::parse_command_line(argc, argv, usage), vm);

    bpo::notify(vm);

    if (vm.count("help")) {
      std::cout << usage << std::endl;
      return 1;
    }
    if (vm.count("version")) {
      std::cout << version << std::endl;
      return 1;
    }
    if (vm.count("consolelog")) {
      config.consolelog(true);
    }

    if (vm.count("lfpath")) {
      //TODO: check directory permission
      // ...
      config.log_file_path = vm["lfpath"].as<std::string>();
    }

    if (vm.count("pospolltimeout")) {
      config.position_poll_timeout = vm["pospolltimeout"].as<int>();
    }

    if (vm.count("dbhost")) {
      config.dbms_host = vm["dbhost"].as<std::string>();
    }

    if (vm.count("dbport")) {
      config.dbms_port = vm["dbport"].as<int>();
    }

    if (vm.count("dbname")) {
      config.dbms_dbname = vm["dbname"].as<std::string>();
    }
    if (vm.count("dblogin")) {
      config.dbms_login = vm["dblogin"].as<std::string>();
    }
    if (vm.count("dbpwd")) {
      config.dbms_pwd = vm["dbpwd"].as<std::string>();
    } else {
      std::cout << "db pwd *must* be obligatory specified" << std::endl;
      std::cout << usage << std::endl;
      return 1;
    }

    if (vm.count("vurl")) {
      config.vurl = vm["vurl"].as<std::string>();
    } else {
      std::cerr << "vurl *must* be obligatory specified" << std::endl;
      std::cerr << usage << std::endl;
      return 1;
    }
    if (vm.count("vurl2")) {
      config.vurl2 = vm["vurl2"].as<std::string>();
    } 
    if (vm.count("vuser")) {
      config.vuser = vm["vuser"].as<std::string>();
    } else {
      std::cerr << "vuser *must* be obligatory specified" << std::endl;
      std::cerr << usage << std::endl;
      return 1;
    }
    if (vm.count("vpwd")) {
      config.vpwd = vm["vpwd"].as<std::string>();
    } else {
      std::cerr << "vpwd *must* be obligatory specified" << std::endl;
      std::cerr << usage << std::endl;
      return 1;
    }
    if (vm.count("log-lievel")) {
      config.log_level = vm["loglevel"].as<int>();
    }

  } catch (const boost::program_options::error & err) {
    std::cerr << "wrong command line option, details: " << err.what() << std::endl;
    return -1;
  }

  return 0;
}

//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  auto cmo_res = setup_cmd_options(argc, argv);
  if (cmo_res !=0)
    return cmo_res;

  setup_logging(config.log_location(), config.log_level);

  // print work configuration
  BOOST_LOG_TRIVIAL(info) << "\nVIST parser config:" << std::endl
                          << "db host: " << config.db_host() << std::endl
                          << "db port: " << config.db_port() << std::endl
                          << "db name: " << config.db_name() << std::endl
                          << "db user: " << config.db_login() << std::endl
                          << "vurl: " << config.vurl << std::endl
                          << "vurl2: " << config.vurl2 << std::endl
                          << "vuser: " << config.vuser << std::endl
                          << "log location: " << config.log_location() << std::endl
                          << "consolelog: " << config.consolelog() << std::endl
                          << "position poll timeout: " << config.position_poll_timeout << " (seconds)" << std::endl
                          << "log level: " << config.log_level << std::endl << std::endl;

  int res = db_init_1(config.db_host().c_str(),
                      config.db_port(),
                      config.db_name().c_str(),
                      config.db_login().c_str(),
                      config.db_pwd().c_str(),
                      nullptr,
                      "vist");

  if (res != DB_RC_SUCCESS) {
    BOOST_LOG_TRIVIAL(error) << "failed to init db, rc=" << res << std::endl;
    return 1;
  }

  try {
    boost::asio::io_service ios;
    boost::asio::deadline_timer t1(ios, boost::posix_time::seconds(10));
    t1.async_wait(boost::bind(handler1, boost::asio::placeholders::error,  &t1));
    ios.run();
  }  catch (std::exception & ex) {
      BOOST_LOG_TRIVIAL(error) << ex.what() << std::endl;
  }

  return 0;
}
