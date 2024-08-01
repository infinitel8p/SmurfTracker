# SmurfTracker
Do you wonder if you are playing against a smurf? This plugin will help you to identify them.  
It uses FlareSolverr to bypass the Cloudflare protection and fetch the wins or mmr of the players in your match.  
If your opponent has few wins in comparison to your lobby and is playing out of his mind, he probably is a smurf.

## Features
- Fetch the wins of all players, all except you or only the opponents in your match (TDB, right now it fetches all players)
- Fetch the mmr of all players in your match - done via scraping so you can see the mmr even in private matches (TBD, uses bakkesmod mmr wrapper for now)

## Installation
### Prerequisites - Custom FlareSolverr
- Get the docker image:
    ```bash
    docker pull infinitel8p/smurftracker_flaresolverr
    ```

- Run it with:
    ```bash
    docker run -d --name flaresolverr -p 8191:8191 smurftracker_flaresolverr
    ```

This should start the server on port 8191.

### SmurfTracker Plugin

- Download the the newest release here: [Releases](https://github.com/infinitel8p/SmurfTracker/releases/latest).
- Move the downloaded .dll file into %appdata%\bakkesmod\bakkesmod\plugins
- Make sure BakkesMod is running and start Rocket League
- Press F2 to open the BakkesMod menu and navigate to the Plugins tab and there open the Plugin Manager
- Locate the Plugin SmurfTracker in the list and click the checkbox to enable it.
- Go back to the Plugins tab and find SmurfTracker in the list, click it and apply your desired settings  

Important: Make sure to set the correct IP of the FlareSolverr instance in the settings.