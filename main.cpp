#include <iostream>
#include <libncclient/c_net_client.h>
#include <libncclient/c_server_api.h>
#include <libncclient/c_shared_data.h>

#include <libncclient/nc_util.hpp>
#include <thread>

void print_thread(c_shared_data shared)
{
    while(1)
    {
        if(sd_has_front_read(shared))
        {
            char* c_data = sd_get_front_read(shared);
            printf("%s\n", c_data);
            free_string(c_data);
        }
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

        /*while(sd_has_front_read(shared))
        {
            char* c_data = sd_get_front_read(shared);
            printf("%s\n", c_data);
            free_string(c_data);
        }*/

        ///its only fine to do this here because the command prompt isn't event driven
        ///normally run this on a clock of 500ms or so
        sa_do_poll_server(shared);
    }

    sd_set_termination(shared);

    return 0;
}
