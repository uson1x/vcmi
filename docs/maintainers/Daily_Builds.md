# Builds downloads server

RAM requirements: non-existing
CPU requirements: non-existing
SSD requirements: 100 GB, ideally - 200 GB. Can be located on a separate volume

## Setup

- Copy scripts from `scripts` directory to `/root/scripts`
- Execute script `setup_uploader.sh`
- If needed, attach volume for builds storage via fstab: `/dev/disk/by-id/scsi-0DO_Volume_vcmi-second-builds /home/downloader ext4 defaults,nofail,discard 0 0`
- Ensure that `/home/downloader` and `/home/downloader/www` are owned by root, while all `/home/downloader/www` content is owned by `downloader` user
- Copy sites definitions from `scripts/sites` to `/etc/nginx/sites-available` and symlink them to `/etc/nginx/sites-enabled`
- Add certificates to nginx
- Install `fancy_index` nginx module: `apt install libnginx-mod-http-fancyindex`
- Restart nginx
