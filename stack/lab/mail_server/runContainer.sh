#!/bin/bash
sudo docker run --name opensmtpd-container -it -p 25:25 -p 55555:55555  --restart unless-stopped opensmtpd-container-image
