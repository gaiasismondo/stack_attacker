
config_line=$(cat ../config.json |grep Manager_IP)
        ipstring=(${config_line//:/ })
        preip=(${ipstring[1]//\"/ })
        MANAGER_IP=${preip[0]}

cp template-san.cnf san.cnf
sed -i 's/MANAGERIP/'"$MANAGER_IP"'/g' san.cnf
