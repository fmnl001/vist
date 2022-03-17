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

static int log_level = 6;

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
  struct ParserStruct *state = (struct ParserStruct *) userData;
  state->tags++;
  state->depth++;

  try {
    if ((strcmp(name, "Technic") == 0)) {
      for (int i=0; atts[i]; i+=2)  {
        if (strcmp(atts[i], "vehicle_export_code") == 0) {
          ti.vec = atts[i+1];
        }
        else if (strcmp(atts[i], "message_id") == 0) {
          ti.mid = std::stoi(atts[i+1]);
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

  /* Get a clean slate for reading in character data. */
  free(state->characters.memory);
  state->characters.memory = nullptr;
  state->characters.size = 0;
}

//------------------------------------------------------------------------------
static void endElement(void *userData,
                       const XML_Char *name)
{
  struct ParserStruct *state = (struct ParserStruct *) userData;
  state->depth--;

  if ((strcmp(name, "Technic") == 0)) {
    tiv.push_back(ti);
    ti.reset_data();
  }
//  printf("%5lu   %10lu   %s\n", state->depth, state->characters.size, name);
}

//------------------------------------------------------------------------------
static void characterDataHandler(void *userData,
                                 const XML_Char *s,
                                 int len)
{
  struct ParserStruct *state = (struct ParserStruct *) userData;
  struct MemoryStruct *mem = &state->characters;

  char *ptr = (char*)realloc(mem->memory, mem->size + len + 1);
  if(!ptr) {
    /* Out of memory. */
    BOOST_LOG_TRIVIAL(fatal) << "Not enough memory (realloc returned NULL)";
    state->ok = 0;
    return;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), s, len);
  mem->size += len;
  mem->memory[mem->size] = 0;
}


//------------------------------------------------------------------------------
static size_t parseStreamCallback(void *contents,
                                  size_t length,
                                  size_t nmemb,
                                  void *userp)
{
  XML_Parser parser = (XML_Parser) userp;
  size_t real_size = length * nmemb;
  struct ParserStruct *state = (struct ParserStruct *) XML_GetUserData(parser);

  // Only parse if we are not already in a failure state.
  if(state->ok && XML_Parse(parser, (const char *)contents, real_size, 0) == 0) {
    auto error_code = XML_GetErrorCode(parser);

    BOOST_LOG_TRIVIAL(error) << "Parsing response buffer of length: "
                             <<  real_size
                             << " with error code: " << error_code
                             << "(" << XML_ErrorString(error_code) << ")"
                             << " failed";
    state->ok = 0;
  }

  return real_size;
}

//------------------------------------------------------------------------------
static void
process_data(std::vector<Technic_info> & tiv)
{
  for (auto & i : tiv) {
    if (vecmidmap[i.vec] != i.mid)
      i.store_to_db();

    vecmidmap[i.vec] = i.mid;
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

      XML_Parser parser;
      struct ParserStruct state;

      // Initialize the state structure for parsing.
      memset(&state, 0, sizeof(struct ParserStruct));
      state.ok = 1;

      // Initialize a namespace-aware parser.
      parser = XML_ParserCreateNS(NULL, '\0');
      XML_SetUserData(parser, &state);
      XML_SetElementHandler(parser, startElement, endElement);
      XML_SetCharacterDataHandler(parser, characterDataHandler);

      curl = curl_easy_init();
      if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://vist_app_kru.ugmk.com/core/__event_state_values_export_view/?export_code");
//        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/97.0.4692.71 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "SyrovAA");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "SyrovAA1");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, parseStreamCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)parser);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
          BOOST_LOG_TRIVIAL(error) << "curl_easy_perform() failed: " << curl_easy_strerror(res);
        }
        else if(state.ok) {
          /* Expat requires one final call to finalize parsing. */
          if(XML_Parse(parser, nullptr, 0, 1) == 0) {
            int error_code = XML_GetErrorCode(parser);
            BOOST_LOG_TRIVIAL(error) << "Finalizing parsing failed with error code "
                                     << error_code
                                     << "(" << XML_ErrorString(XML_Error(error_code)) << ")";
          }
          else {
//            printf("                     --------------\n");
//            printf("                     %lu tags total\n", state.tags);
          }
        }

        /* Clean up. */
        free(state.characters.memory);
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
  const std::string version{"CARETRACK parser v0.1"};

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
      ("consolelog", "skip logging to console")
      ("pospolltimeout,ppt", bpo::value<int>(), "position poll timeout");

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

  setup_logging(config.log_location(),
                log_level);

  // print work configuration
  BOOST_LOG_TRIVIAL(info) << "\nCARETRACK parser config:" << std::endl
                          << "db host: " << config.db_host() << std::endl
                          << "db port: " << config.db_port() << std::endl
                          << "db name: " << config.db_name() << std::endl
                          << "db user: " << config.db_login() << std::endl;


  int res = db_init_1(config.db_host().c_str(),
                      config.db_port(),
                      config.db_name().c_str(),
                      config.db_login().c_str(),
                      config.db_pwd().c_str(),
                      nullptr,
                      "caretrack");

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
