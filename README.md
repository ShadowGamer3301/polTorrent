# polTorrent
CLI torrent client for unix systems

## Legal notice
Authors/developers of polTorrent or any of it's direvatives are not responsible for any legal issues that you may suffer from downloading illegal/pirated content

## About project
polTorrent is text based bittorrent client that allows you to download files using bittorrent protocol. At first it was only developed for personal use but later
I've decided to release it as FOSS project. Project is curently developed only by me but if you want to help, feel free to join. For now polTorrent is only avaliable for 
Linux/Unix systems (in theory it should be able to compile on Windows but I haven't tested it). 

## Ports
As stated before polTorrent is only avaliable of Linux/Unix systems. When it comes to windows I'm planing to release a port within 6 months, but I'm not planing to release
any ports for MacOS, IOS and Android. 

## Example of usage
To use polTorrent just type
```SHELL
./polTorrent <magnet-url>
```
Additional parameters
- '-v' - Turns on verbose mode which displays information about peers (IP, client, transfer speed, etc.)
- '-s' - Enables custom save path (although session and resume file still stay in user home directory)

## Future plans
- Add ability to use .torrent files
- Support for multiple torrents
- Port application to Windows
- Show better data (peers, seeding data, etc.)
