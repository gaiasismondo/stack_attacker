echo "STARTING METASPLOIT SCRIPTS..."
while true; do
  read -p "Do you want to use SuperScript for the IoT Laboratory or the Security Laboratory? [I/S]: " var
  case $var in
    [Ii]* ) msfconsole -q -r /opt/metasploit-framework/embedded/framework/resource/resource_labIoT/resource_script_caller_LP.rc /opt/metasploit-framework/embedded/framework/super/config.json;exit;;
    [Ss]* ) msfconsole -q -r /opt/metasploit-framework/embedded/framework/resource/resource_labSic/resource_script_caller_LP.rc /opt/metasploit-framework/embedded/framework/super/config.json;exit;;
    * ) echo "Your answer is wrong, please retry.";;
  esac
done
echo "EXECUTION COMPLETED"

