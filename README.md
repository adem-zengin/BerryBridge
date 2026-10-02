# <img width="114" height="114" alt="beeper114" src="https://github.com/user-attachments/assets/9658bb61-5c5c-4d61-b045-c036e2fdeac1" align="center" /> Berry Bridge

**Berry Bridge** is a native unofficial Beeper client for rooted BlackBerry 10 devices that synchronizes your messaging accounts via the Beeper Desktop API.

## 📱 Screenshots

<table>
  <tr>
    <td width="33%"><img src="https://github.com/user-attachments/assets/de06ed7f-2a49-4c55-9be2-2857585999b5" alt="1" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/90861b61-60aa-4c00-ac91-abf4db7fcf65" alt="2" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/f1dcec03-d671-4ae6-bed4-1aa1478c5a81" alt="3" /></td>
  </tr>
  <tr>
    <td width="33%"><img src="https://github.com/user-attachments/assets/3afe5d47-6f1b-4d3e-a8a6-f7313baf9f7c" alt="4" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/9d7dd451-42d9-4813-abcd-950f1fa4139e" alt="5" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/4f1a4517-ce6f-44dc-b9eb-8080bcf7299d" alt="6" /></td>
  </tr>
  <tr>
    <td width="33%"><img src="https://github.com/user-attachments/assets/f2a3cc8e-b2b4-442a-b2d5-d061da0928b8" alt="7" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/abe9740b-2166-49e8-8b6c-43489a204a8d" alt="8" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/acc44d0d-001c-402d-af72-c3d70d87169b" alt="9" /></td>
  </tr>
  <tr>
    <td width="33%"><img src="https://github.com/user-attachments/assets/ee81c9e5-6a45-43f8-9335-7f564153f2fb" alt="9.5" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/eefeb098-5b4c-4098-9b5e-29e10ed057a2" alt="10" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/8fef73ee-a429-4c3c-8367-39864bb9cec0" alt="11" /></td>
  </tr>
  <tr>
    <td width="33%"><img src="https://github.com/user-attachments/assets/a0524f64-46cf-4ddc-92f8-4ff4f50f3a71" alt="12" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/03c90992-4b75-4bc8-8e08-be76abcc063e" alt="13" /></td>
    <td width="33%"><img src="https://github.com/user-attachments/assets/3be43ced-ba77-41a5-bce4-e6bacefd403d" alt="14" /></td>
  </tr>
</table>
To use this application, you must first set up the Beeper Desktop application on a PC (for local network use) or on a cloud server (for remote access anywhere). Key features of the application and detailed setup instructions are provided below.

## ✨ Key Features

- **Multi-Account Tabbed UI:** Every linked account gets its own dedicated tab.
- **Custom Color Palettes:** Easily distinguish between different accounts with customizable theme palettes.
- **Battery-Efficient Push Service:** Native background connectivity designed for minimal battery consumption.
- **Rich Messaging:** Send and receive text messages, media attachments, and files.
- **BlackBerry Hub Notifications:** Receive native system notifications directly in the BB10 Hub.
- **Chat Management:** Mute, pin, edit, delete, react to, and reply to specific messages.
- **Message Status Indicators:** Clear indicators for **Pending**, **Sent**, and **Seen** states.
- **Secure Communication:** Encrypted HTTPS/TLS connections between your BB10 device and the Beeper Desktop API.

## 🏠 Option 1: Windows PC Setup (LAN / Home & Office Use)

Follow these steps or watch the setup guide on YouTube to connect your BlackBerry 10 device to Beeper Desktop running on your local Windows PC.

[![## 📸 Video Tutorial](https://img.youtube.com/vi/W7NVCAgfY6s/0.jpg)](https://www.youtube.com/watch?v=W7NVCAgfY6s)

### 1. Configure Beeper Desktop
1. Download and install **Beeper Desktop** on your Windows PC.
2. Sign in / Sign up and link your messaging accounts.
3. Go to **Settings > Integrations > Advanced**.
4. Enable **Remote Access** and restart the app.
5. Go to **Settings > Integrations > Approved Connections**.
6. Click the **`+`** icon to add a new connection.
7. Enter a name for your access token.
8. Set **Expires In** to **NEVER**.
9. Set **Allow sensitive actions** to **YES**.
10. Click **Create**. Copy the generated token and save it to a text file. *(This will be your Access Token)*.

### 2. Find Your PC's Local IP Address
1. Open Command Prompt (`cmd`) on your PC and run `ipconfig`.
2. Find your **IPv4 Address** (e.g., `192.168.1.50`).
3. Your **Server URL** will use port `23373` in the following format:
   ```text
   http://YOUR_LOCAL_IPV4:23373
   ```

### 3. Configure Windows Firewall
1. Open the Windows Start menu, search for **Windows Security**, and launch it.
2. Go to **Firewall & network protection** > click **Advanced settings**.
3. Select **Inbound Rules** in the left menu, then click **New Rule...** in the right menu.
4. Select **Port** and click **Next**.
5. Choose **TCP** and enter `23373` under **Specific local ports**, then click **Next**.
6. Select **Allow the connection** and click **Next**.
7. Uncheck **Public** (keep *Domain* and *Private* checked) and click **Next**.
8. Name the rule (e.g., `Beeper Remote Access`) and click **Finish**.

### 4. Setup BlackBerry 10 Device
1. Send the **Server URL** and **Access Token** to your BB10 device (via email).
2. Launch **Berry Bridge** and navigate to the **Settings** page.
3. Copy and paste the **Server URL** and **Access Token**.
4. Tap **List Accounts** to connect and sync!

---

## ☁️ Option 2: Free Cloud Server Setup (Global Access)

To access your messages outside your local network without keeping a home PC constantly running, you can host Beeper Desktop on a free cloud server (e.g., Oracle Cloud Always Free).

### 1. Create an Oracle Cloud VM
1. Sign up for a free **Oracle Cloud** account.
2. Create a Virtual Machine (VM) eligible under the **Always Free** tier (e.g., Ubuntu ARM64).
3. **Save your SSH private key** during instance creation.
4. Once created, note down your server's **Public IP Address**.

### 2. Open Ports in Oracle Cloud VCN Security Rules
1. Go to **Instances** > Click on your instance name.
2. Under **Instance details**, click on your **Virtual Cloud Network (VCN)**.
3. Select **Subnets** > Click on your subnet.
4. Click on the **Default Security List**.
5. Under **Ingress Rules**, click **Add Ingress Rules**.
6. Add rules allowing incoming TCP traffic on ports **`80`**, **`443`**, and **`3389`**.

### 3. Server Configuration over SSH
Connect to your server via SSH (e.g., using Terminal or PuTTY):

```bash
ssh -i /path/to/your_private_key.key ubuntu@YOUR_PUBLIC_IP
```

Set a password for the `ubuntu` user:
```bash
sudo passwd ubuntu
```

Install the XFCE desktop environment, XRDP (Remote Desktop), and Firefox:
```bash
sudo apt update
sudo apt install xfce4 xfce4-goodies xrdp firefox -y
echo "xfce4-session" > ~/.xsession
sudo systemctl restart xrdp
```

Open system firewall ports:
```bash
sudo iptables -I INPUT -p tcp -m multiport --dports 80,443,3389 -j ACCEPT
sudo netfilter-persistent save
```

### 4. Dynamic DNS & SSL Certificate Setup
1. Register a free domain/hostname (e.g., via [DuckDNS](https://www.duckdns.org/)) and point it to your server's **Public IP Address**.
2. Install Nginx and Certbot:
   ```bash
   sudo apt install nano nginx certbot python3-certbot-nginx -y
   ```
3. Generate an SSL certificate:
   ```bash
   sudo certbot certonly --nginx -d YOUR_DOMAIN.duckdns.org --key-type rsa
   ```

### 5. Configure Nginx Reverse Proxy
Create the Nginx configuration file:
```bash
sudo nano /etc/nginx/sites-available/beeper
```

Paste the following configuration (replace `YOUR_DOMAIN.duckdns.org` with your actual domain):

```nginx
server {
    listen 443 ssl;
    server_name berrybeeper.duckdns.org;

    client_max_body_size 500M;

    ssl_certificate /etc/letsencrypt/live/YOUR_DOMAIN.duckdns.org/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/YOUR_DOMAIN.duckdns.org/privkey.pem;

    ssl_protocols TLSv1 TLSv1.1 TLSv1.2 TLSv1.3;
    ssl_prefer_server_ciphers on;
    ssl_ciphers 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-SHA:@SECLEVEL=0';
    access_log /var/log/nginx/beeper_access.log ssl_verbose;
    error_log /var/log/nginx/beeper_error.log info;

    location / {
        proxy_pass http://127.0.0.1:23373;
        proxy_http_version 1.1;

        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        proxy_read_timeout 86400s;
        proxy_send_timeout 86400s;
    }
}
```

Enable the site configuration and restart Nginx:
```bash
sudo ln -s /etc/nginx/sites-available/beeper /etc/nginx/sites-enabled/
sudo rm -f /etc/nginx/sites-enabled/default
sudo systemctl restart nginx
```

### 6. Connect via RDP & Install Beeper Desktop
1. Launch **Remote Desktop Connection** on your PC.
2. Connect to your server's **Public IP Address**.
3. Log in with Username: `ubuntu` and the password set earlier.
4. Open **Firefox** inside the remote desktop session.
5. Download Beeper Desktop v4.3.113:
   ```text
   https://beeper-desktop.download.beeper.com/builds/Beeper-4.3.113-arm64.AppImage
   ```
   > **Note:** Version `4.3.113` is currently the latest verified working version for headless/server setups.

6. Open the **Downloads** folder, right-click the `.AppImage` file > **Properties** > **Permissions** > Check **Allow this file to run as a program**.
7. Double-click the file to launch **Beeper Desktop**.
8. Sign in, link your messaging accounts, and enable **Remote Access** under **Settings > Integrations > Advanced**.
9. Generate your **Access Token** under **Settings > Integrations > Approved Connections** (Set *Expires In* to **NEVER** and *Allow sensitive actions* to **YES**). Save this token securely.

### 7. Connect Berry Bridge on BlackBerry 10
1. Open **Berry Bridge** on your BB10 device and go to **Settings**.
2. Enter your domain name as the **Server URL** (e.g., `https://YOUR_DOMAIN.duckdns.org`).
3. Paste your generated **Access Token**.
4. Tap **List Accounts** and enjoy global access!

## Support
[Patreon](https://www.patreon.com/16129770/join)
