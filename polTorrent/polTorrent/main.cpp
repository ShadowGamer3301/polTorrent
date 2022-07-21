//STD includes
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <csignal>
#include <sstream>

//Spdlog includes
#include <spdlog/spdlog.h>

//Libtorrent includes
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>

using clk = std::chrono::steady_clock;


namespace pT
{
    //Variables that will allow to save files to custom directories
    std::string g_SavePath = "."; //By default it's current directory
    
    inline void HandleOutputPath(std::string save_path) { g_SavePath = save_path; }
    
    inline void HandleArguments(int argc, char** argv)
    {
        std::stringstream ss;
        ss << "Detected " << argc-2 << " arguments";
        
        spdlog::info(ss.str());
        
        ss.str(std::string()); //Clear stringstream
        
        for(int i = 2; i < argc; i++) //We start from the third argument since 1st is application name and 2nd is torrent url
        {
            ss << "Detected " << argv[i] << " argument";
            spdlog::info(ss.str());
            ss.str(std::string()); //Clear stringstream
            
            //Detect which argument we are dealing with
            if(std::strcmp(argv[i], "-s")) //Custom save path
            {
                HandleOutputPath(argv[i+1]);
                i++;
            }
        }
        
    }
    
    //Displays simple hello message
    inline void HelloMsg() {
        spdlog::info("polTorrent by Zbigniew Przybyła");
        spdlog::info("Open beta v0.0.2");
        spdlog::info("Released under GNU GPLv3 license");
    }
    
    //Returns text describing torrent status
    inline char const* TorrentState(lt::torrent_status::state_t s) {
        switch(s)
        {
            case lt::torrent_status::checking_files: return "Checking files integrity";
            case lt::torrent_status::downloading_metadata: return "Downloading metadata";
            case lt::torrent_status::downloading: return "Downloading files";
            case lt::torrent_status::finished: return "Finished";
            case lt::torrent_status::seeding: return "Seeding";
            case lt::torrent_status::checking_resume_data: return "Checking resume data";
            default: return "<>";
        }
    }
    
    //Loads data from file (It does not load .torrent files)
    inline std::vector<char> load_file(char const* filename) {
        std::ifstream ifs(filename, std::ios_base::binary);
        ifs.unsetf(std::ios_base::skipws);
        return {std::istream_iterator<char>(ifs), std::istream_iterator<char>()};
    }
    
    //Set when we are exiting application
    std::atomic<bool> Shut_Down{false};
    
    inline void SigHandler(int) {Shut_Down = true;}

    inline void ShowDownloadUploadState(lt::torrent_status s)
    {
        std::cout << '\r' << TorrentState(s.state) << ' '
        << (s.download_payload_rate / 1000) << " kB/s "
        << (s.total_done / 1000) << " kB ("
        << (s.progress_ppm / 10000) << "%) downloaded ("
        << s.num_peers << " peers)\x1b[K";
        std::cout.flush();
    }
    
    inline void Cleanup(lt::session& ses)
    {
        spdlog::info("Saving session state");
        std::ofstream of(".session", std::ios_base::binary);
        of.unsetf(std::ios_base::skipws);
        auto const b = libtorrent::write_session_params_buf(ses.session_state(), lt::save_state_flags_t::all());
        of.write(b.data(), int(b.size()));
    }
    
    inline void DownloadTorrent(int argc, char** argv) {
        //Load session parameters
        auto session_params = load_file(".session");
        lt::session_params params = session_params.empty() ? lt::session_params() : lt::read_session_params(session_params);
        params.settings.set_int(lt::settings_pack::alert_mask, lt::alert_category::error | lt::alert_category::storage | lt::alert_category::status);
        lt::session ses(params);
        clk::time_point last_save_resume = clk::now();
        
        //Load resume data from disk and pass it in as we add the magnet link
        auto buf = load_file(".resume_file");
        lt::add_torrent_params magnet = lt::parse_magnet_uri(argv[1]);
        if(buf.size()) {
            lt::add_torrent_params atp = lt::read_resume_data(buf);
            if(atp.info_hashes == magnet.info_hashes) magnet = std::move(atp);
        }
        
        magnet.save_path = g_SavePath; //Save in current directory (TODO: Add custom save path)
        
        ses.async_add_torrent(std::move(magnet));

        //This is the handler that will be set once we get notification of it being added
        lt::torrent_handle h;
        
        std::signal(SIGINT, &SigHandler);
        
        //Set when we're exiting
        bool done = false;
        for(;;) {
            std::vector<lt::alert*> alerts;
            ses.pop_alerts(&alerts);
            
            if(Shut_Down) {
                Shut_Down = false;
                auto const handles = ses.get_torrents();
                if(handles.size() == 1) {
                    handles[0].save_resume_data(lt::torrent_handle::save_info_dict);
                    done = true;
                }
            }
            
            for (lt::alert const* a : alerts) {
                if(auto at = lt::alert_cast<lt::add_torrent_alert>(a)) {
                    h = at->handle;
                }
                
                //If we recieve finished alert or an error we're done
                if(lt::alert_cast<lt::torrent_finished_alert>(a)) {
                    h.save_resume_data(lt::torrent_handle::save_info_dict);
                    spdlog::info("Torrent sucessfully downloaded, seeding has started");
                }
                
                if(lt::alert_cast<lt::torrent_error_alert>(a)) {
                    spdlog::error(a->message());
                    done = true;
                    h.save_resume_data(lt::torrent_handle::save_info_dict);
                }
                
                //When resume data is ready, save it
                if (auto rd = lt::alert_cast<lt::save_resume_data_alert>(a)) {
                    std::ofstream of(".resume_file", std::ios_base::binary);
                    of.unsetf(std::ios_base::skipws);
                    auto const b = write_resume_data_buf(rd->params);
                    of.write(b.data(), int(b.size()));
                     if (done) {pT::Cleanup(ses); return;}
                }
                
                if (lt::alert_cast<lt::save_resume_data_failed_alert>(a)) {
                    if (done) {pT::Cleanup(ses); return;}
                }
                
                if (auto st = lt::alert_cast<lt::state_update_alert>(a)) {
                    if (st->status.empty()) continue;
                    
                    //At this point only single torrent is handled so it's known
                    // which one the status is for
                    
                    lt::torrent_status const& s = st->status[0];
                    ShowDownloadUploadState(s);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                
                //Ask the session to update state output for the torrent
                ses.post_torrent_updates();
                
                //Save resume data once every 30 seconds
                if(clk::now() - last_save_resume > std::chrono::seconds(30)) {
                    h.save_resume_data(lt::torrent_handle::save_info_dict);
                    last_save_resume = clk::now();
                }
                
            }
        }
    }
    
    
}

int main(int argc, char** argv) try
{
    pT::HelloMsg(); //Show hello message
    
    //Check if valid number of arguments was passed
    if(argc < 2) {
        spdlog::critical("Invalid number of arguments were passed");
        spdlog::critical("Usage: ./polTorrent <magnet-url>");
        return 1;
    }
    
    pT::HandleArguments(argc, argv);
    pT::DownloadTorrent(argc, argv);
    
    
    return 0;
}
catch(std::exception& e)
{
    spdlog::critical(e.what());
}