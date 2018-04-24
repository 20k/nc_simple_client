#include <iostream>
#include <libncclient/c_net_client.h>
#include <libncclient/c_server_api.h>
#include <libncclient/c_shared_data.h>

#include <libncclient/nc_util.hpp>
#include <libncclient/nc_string_interop.hpp>
#include <thread>
#include <map>
#include <cstring>

void print_thread(c_shared_data shared)
{
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> found_args;
    std::map<std::string, bool> is_valid;

    while(1)
    {
        if(sd_has_front_read(shared))
        {
            ///This needs to return length as well
            char* c_data = sd_get_front_read(shared);

            server_command_info command_info = sa_server_response_to_info(c_data, std::strlen(c_data));

            std::string to_print;

            if(command_info.type == server_command_command)
            {
                to_print = c_str_consume(sa_command_to_human_readable(command_info));
            }

            if(command_info.type == server_command_chat_api)
            {
                std::vector<std::string> chnls;
                std::vector<std::string> msgs;

                std::vector<std::string> in_channels;

                chat_api_info chat_info = sa_chat_api_to_info(command_info);

                for(int i=0; i < chat_info.num_msgs; i++)
                {
                    chnls.push_back(c_str_to_cpp(chat_info.msgs[i].channel));
                    msgs.push_back(c_str_to_cpp(chat_info.msgs[i].msg));
                }

                for(int i=0; i < chat_info.num_in_channels; i++)
                {
                    in_channels.push_back(c_str_to_cpp(chat_info.in_channels[i].channel));
                }

                std::string str = "Joined Channels: ";

                for(int i=0; i < chat_info.num_in_channels; i++)
                {
                    str += in_channels[i] + " ";
                }

                str += "\n";

                for(int i=0; i < chat_info.num_msgs; i++)
                {
                    str += chnls[i] + " : " + msgs[i];
                }

                to_print = str;

                sa_destroy_chat_api_info(chat_info);
            }

            if(command_info.type == server_command_server_scriptargs)
            {
                script_argument_list args = sa_server_scriptargs_to_list(command_info);

                if(args.scriptname != nullptr)
                {
                    std::vector<std::pair<std::string, std::string>> auto_args;

                    for(int i=0; i < args.num; i++)
                    {
                        std::string key = c_str_to_cpp(args.args[i].key);
                        std::string val = c_str_to_cpp(args.args[i].val);

                        auto_args.push_back({key, val});
                    }

                    std::string scriptname = c_str_to_cpp(args.scriptname);

                    std::string str = "script " + scriptname + " takes args ";

                    for(auto& i : auto_args)
                    {
                        str = str + i.first + " " + i.second;
                    }

                    str = str + "\n";

                    to_print = str;

                    found_args[scriptname] = auto_args;
                    is_valid[scriptname] = true;
                }

                sa_destroy_script_argument_list(args);
            }

            if(command_info.type == server_command_server_scriptargs_invalid)
            {
                std::string name = c_str_consume(sa_server_scriptargs_invalid_to_script_name(command_info));

                if(name.size() > 0)
                {
                    is_valid[name] = false;
                }
            }

            free_string(c_data);

            sa_destroy_server_command_info(command_info);

            if(to_print.size() > 0)
            {
                printf("%s\n", to_print.c_str());
            }
        }

        /*if(command_info.type == server_command_server_scriptargs_ratelimit)
        {
            std::string name = c_str_consume(sa_server_scriptargs_ratelimit_to_script_name(command_info));

            if(name.size() > 0)
            {
                found_unprocessed_autocompletes.insert(name);
            }
        }*/
    }
}

int main()
{
    c_shared_data shared = sd_alloc();

    ///must be key.key as currently the server internally expects this to exist
    ///will be fixed in a later revision
    if(file_exists("key.key"))
    {
        std::string fauth = read_file_bin("key.key");

        sd_set_auth(shared, fauth.c_str());
    }

    nc_start(shared, "77.96.132.101", "6760");

    std::thread(print_thread, shared).detach();

    bool running = true;

    while(running)
    {
        std::string command;
        std::getline(std::cin, command);

        ///hack, to be fixed in a later version
        std::string swapping_users = "user ";

        if(command.substr(0, swapping_users.length()) == swapping_users)
        {
            std::vector<std::string> spl = no_ss_split(command, " ");

            if(spl.size() >= 2)
            {
                sd_set_user(shared, spl[1].c_str());
            }
        }

        if(sa_is_local_command(command.c_str()))
        {
            sd_add_back_read(shared, "Unhandled Local Command");
        }
        else
        {
            char* current_user = sd_get_user(shared);
            char* up_handled = sa_default_up_handling(current_user, command.c_str(), "./scripts/");
            char* server_command = sa_make_generic_server_command(up_handled);

            sd_add_back_write(shared, server_command);

            free_string(server_command);
            free_string(up_handled);
            free_string(current_user);
        }

        ///its only fine to do this here because the command prompt isn't realtime
        ///normally run this on a clock of 500ms or so
        ///kind of hacky, TODO: provide a clock
        sa_do_poll_server(shared);
    }

    sd_set_termination(shared);

    return 0;
}
