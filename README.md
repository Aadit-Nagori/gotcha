# Gotcha! - Real-time Multiplayer WebSocket Game

A real-time multiplayer guessing game built with C (libwebsockets) backend and JavaScript frontend. I built this purely as a fun exercise to learn websockets and the basics of network programming. As such, there are quite a few known limitations and probably many more unknown ones but i was able to achieve my goal (of learning) through it so at this stage i most likely will not be updating this codebase.

## Game Rules

Gotcha is a 2-player turn-based game where players alternate between being the **Guesser** and **Mover**:

- **Guesser**: Tries to match the opponent's move to score points
- **Mover**: Tries to be unpredictable to avoid being guessed
- Both players submit moves simultaneously (Up/Down/Left/Right)
- If moves match: Guesser gets a point, roles stay the same
- If moves don't match: Roles switch
- Game ends after 5 rounds, highest score wins

## Features

- **Host/Join System**: Create rooms with 4-digit codes
- **Real-time Updates**: Live score tracking and role switching
- **Modern UI**: Clean, responsive web interface
- **Multiple Input Methods**: Mouse clicks or keyboard (WASD/Arrow keys)
- **Game Log**: Timestamped event history

## Architecture

### Backend (C + libwebsockets)
- WebSocket server handling multiple concurrent games
- Room-based game management (supports 10,000 concurrent rooms)
- Real-time message passing and game state synchronization
- Memory-efficient player and session management

### Frontend (HTML/CSS/JavaScript)
- Modern gradient UI with smooth animations
- Real-time WebSocket communication
- Responsive design with visual feedback
- Game state management and role indicators

## Prerequisites

- CMake 3.5+
- libwebsockets development libraries
- Modern web browser with WebSocket support

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libwebsockets-dev
```

**macOS:**
```bash
brew install libwebsockets cmake
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

1. Start the server:
```bash
./gotcha-server
```
Server starts on port 7681 by default.

2. Open the game in your browser:
```
file:///path/to/testing_server.html
```

3. Connect and play:
   - Click "Connect" to connect to the server
   - Click "Host Game" to create a room and get a 4-digit code
   - Share the code with another player who can "Join Game"
   - Start playing once both players are connected

## Project Structure

```
.
├── CMakeLists.txt              # Build configuration
├── server.c                    # Main server entry point
├── protocol_server.c           # WebSocket protocol implementation
├── testing_server.html         # Game interface
├── gotcha.css                  # Styling
├── script.js                   # Frontend game logic
└── README.md                   # This file
```

## Message Protocol

The game uses simple text-based WebSocket messages:

**Client → Server:**
- `HOST` - Create new game room
- `JOIN:1234` - Join room with code 1234
- `MOVE:U` - Submit move (U/D/L/R)

**Server → Client:**
- `ROOM:1234` - Room created with code 1234
- `ROUND_START:GUESSER:1` - Round starts, player 1 is guesser
- `SCORE:2:3` - Current score (P1: 2, P2: 3)
- `ROUND_RESULT:P1_GUESSER_WIN:SCORE:3:3` - Round result with updated scores
- `GAME_OVER:P1_WINS:SCORE:5:2` - Game finished

## Configuration

Default server port can be changed with `-p` flag:
```bash
./gotcha-server -p 8080
```

For production deployment, update the WebSocket URL in `script.js`:
```javascript
ws = new WebSocket('wss://yourdomain.com:7681', 'gotcha-server');
```

## Development

The codebase uses:
- **libwebsockets**: For WebSocket server implementation
- **CMake**: Build system
- **Modern JavaScript**: ES6+ features for frontend
- **CSS Grid/Flexbox**: Responsive layout

Key files to modify:
- `protocol_server.c`: Game logic and message handling
- `script.js`: Frontend game state and UI updates
- `gotcha.css`: Visual styling and animations

## Known Limitations

- No reconnection handling for disconnected players
- No spectator mode or game replay features
- Single-server architecture (no horizontal scaling)


## License

This project is released under the Creative Commons CC0 1.0 Universal Public Domain Dedication.

## Acknowledgments

Built using libwebsockets library for WebSocket server implementation.

