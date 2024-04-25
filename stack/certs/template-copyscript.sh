sudo mkdir /etc/docker/certs.d/MANAGERIP:5000
sudo cp domain.crt /etc/docker/certs.d/MANAGERIP:5000/ca.crt
sudo cp domain.crt /usr/local/share/ca-certificates/ca.crt
