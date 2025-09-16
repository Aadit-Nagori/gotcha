let ws = null;
        let gameState = {
            connected: false,
            inGame: false,
            isHost: false,
            roomCode: null,
            yourScore: 0,
            opponentScore: 0,
            role: null,
            playerNumber: null
        };
        
        // DOM elements
        const elements = {
            log: document.getElementById('log'),
            status: document.getElementById('status'),
            statusDot: document.getElementById('statusDot'),
            input: document.getElementById('input'),
            roomInfo: document.getElementById('roomInfo'),
            roomCode: document.getElementById('roomCode'),
            roleIndicator: document.getElementById('roleIndicator'),
            yourScore: document.getElementById('yourScore'),
            opponentScore: document.getElementById('opponentScore'),
            connectBtn: document.getElementById('connect'),
            hostBtn: document.getElementById('host'),
            joinBtn: document.getElementById('join'),
            upBtn: document.getElementById('up'),
            downBtn: document.getElementById('down'),
            leftBtn: document.getElementById('left'),
            rightBtn: document.getElementById('right')
        };

        function appendLog(message, type = 'info') {
            const div = document.createElement('div');
            div.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
            if (type === 'error') div.style.color = '#dc3545';
            if (type === 'success') div.style.color = '#28a745';
            if (type === 'warning') div.style.color = '#ffc107';
            elements.log.appendChild(div);
            elements.log.scrollTop = elements.log.scrollHeight;
        }
        
        function updateUI() {
            elements.status.textContent = gameState.connected ? 'Connected' : 'Disconnected';
            elements.statusDot.classList.toggle('connected', gameState.connected);
            elements.connectBtn.textContent = gameState.connected ? 'Disconnect' : 'Connect';
            
            elements.hostBtn.disabled = !gameState.connected || gameState.inGame;
            elements.joinBtn.disabled = !gameState.connected || gameState.inGame;
            
            const moveBtnsEnabled = gameState.connected && gameState.inGame;
            elements.upBtn.disabled = !moveBtnsEnabled;
            elements.downBtn.disabled = !moveBtnsEnabled;
            elements.leftBtn.disabled = !moveBtnsEnabled;
            elements.rightBtn.disabled = !moveBtnsEnabled;
            
            elements.yourScore.textContent = gameState.yourScore;
            elements.opponentScore.textContent = gameState.opponentScore;
            
            if (gameState.roomCode) {
                elements.roomInfo.style.display = 'block';
                elements.roomCode.textContent = gameState.roomCode;
            } else {
                elements.roomInfo.style.display = 'none';
            }
            
            if (gameState.role) {
                elements.roleIndicator.style.display = 'block';
                elements.roleIndicator.textContent = gameState.role === 'guesser' ? 
                    'You are the GUESSER - try to match your opponent!' : 
                    'You are the MOVER - try to be unpredictable!';
                elements.roleIndicator.className = `role-indicator ${gameState.role}`;
            } else {
                elements.roleIndicator.style.display = 'none';
            }
        }

        function sendMove(direction) {
            if (!ws || !gameState.inGame) return;
            
            const message = `MOVE:${direction}`;
            ws.send(message);
            appendLog(`Sent move: ${direction}`, 'info');
        }

        // Event listeners
        elements.connectBtn.addEventListener('click', function() {
            if (ws) {
                ws.close();
                return;
            }

            ws = new WebSocket('ws://localhost:7681', 'gotcha-server');
            
            ws.onopen = function() {
                gameState.connected = true;
                updateUI();
                appendLog('Connected to server', 'success');
            };
            
            ws.onclose = function() {
                gameState.connected = false;
                gameState.inGame = false;
                gameState.roomCode = null;
                gameState.role = null;
                updateUI();
                appendLog('Disconnected from server', 'warning');
                ws = null;
            };
            
            ws.onmessage = function(evt) {
                const message = evt.data;
                appendLog(`Received: ${message}`, 'info');
                
                // Parse server messages
                if (message.startsWith('ROOM:')) {
                    gameState.roomCode = message.split(':')[1];
                    gameState.isHost = true;
                    gameState.playerNumber = 1;
                    appendLog(`Hosting room ${gameState.roomCode}`, 'success');

                } else if (message.startsWith('ROUND_START:')) {
                    // Handle round start with role info
                    const parts = message.split(':');
                    const currentGuesser = parseInt(parts[2]);

                    if (gameState.playerNumber === null){
                        gameState.playerNumber = 2;
                    }
                    
                    // Determine YOUR role based on YOUR player number
                    if (gameState.playerNumber === currentGuesser) {
                        gameState.role = 'guesser';
                    } else {
                        gameState.role = 'mover';
                    }

                
                    if (!gameState.inGame) {
                        gameState.inGame = true;
                        appendLog('Game started!', 'success');
                    }
                    updateUI();

                } else if (message.startsWith('SCORE:')) {
                    // Handle score updates
                    const parts = message.split(':');
                    const p1Score = parseInt(parts[1]);
                    const p2Score = parseInt(parts[2]);
                    
                    if (gameState.playerNumber === 1) {
                        gameState.yourScore = p1Score;
                        gameState.opponentScore = p2Score;
                    } else {
                        gameState.yourScore = p2Score;
                        gameState.opponentScore = p1Score;
                    }

                } else if (message.startsWith('ROUND_RESULT:')) {
                    // Handle round results like "ROUND_RESULT:P1_GUESSER_WIN:SCORE:3:2"
                    const parts = message.split(':');
                    const result = parts[1];
                    const p1Score = parseInt(parts[3]);
                    const p2Score = parseInt(parts[4]);
                    
                    if (gameState.playerNumber === 1) {
                        gameState.yourScore = p1Score;
                        gameState.opponentScore = p2Score;
                    } else {
                        gameState.yourScore = p2Score;
                        gameState.opponentScore = p1Score;
                    }
                    
                    appendLog(`Round result: ${result}`, 'info');
                    updateUI();
                } else if (message.startsWith('GAME_OVER:')) {
                        const parts = message.split(':');
                        const result = parts[1];
                        const p1Score = parseInt(parts[3]);
                        const p2Score = parseInt(parts[4]);
                        
                        // Update final scores
                        if (gameState.playerNumber === 1) {
                            gameState.yourScore = p1Score;
                            gameState.opponentScore = p2Score;
                        } else {
                            gameState.yourScore = p2Score;
                            gameState.opponentScore = p1Score;
                        }
                        
                        // End the game
                        gameState.inGame = false;
                        gameState.role = null;
                        
                        // Show game over message
                        if (result === 'P1_WINS') {
                            const winner = gameState.playerNumber === 1 ? 'You won!' : 'You lost!';
                            appendLog(`Game Over! ${winner}`, gameState.playerNumber === 1 ? 'success' : 'error');
                        } else if (result === 'P2_WINS') {
                            const winner = gameState.playerNumber === 2 ? 'You won!' : 'You lost!';
                            appendLog(`Game Over! ${winner}`, gameState.playerNumber === 2 ? 'success' : 'error');
                        } else {
                            appendLog('Game Over! It\'s a tie!', 'info');
                        }
                        
                        updateUI();
                    }
            };
            
            ws.onerror = function(evt) {
                appendLog('Connection error', 'error');
            };
        });

        elements.hostBtn.addEventListener('click', function() {
            if (!ws) return;
            ws.send('HOST');
            appendLog('Requesting to host game...', 'info');
        });

        elements.joinBtn.addEventListener('click', function() {
            if (!ws) return;
            
            const roomCode = elements.input.value.trim();
            if (!roomCode) {
                appendLog('Please enter a room code', 'error');
                return;
            }
            
            ws.send(`JOIN:${roomCode}`);
            appendLog(`Attempting to join room ${roomCode}...`, 'info');
            elements.input.value = '';
        });

        // Direction button event listeners
        elements.upBtn.addEventListener('click', () => sendMove('U'));
        elements.downBtn.addEventListener('click', () => sendMove('D'));
        elements.leftBtn.addEventListener('click', () => sendMove('L'));
        elements.rightBtn.addEventListener('click', () => sendMove('R'));

        // Keyboard controls
        document.addEventListener('keydown', function(e) {
            if (!gameState.inGame) return;
            
            switch(e.key) {
                case 'ArrowUp':
                case 'w':
                case 'W':
                    e.preventDefault();
                    sendMove('U');
                    break;
                case 'ArrowDown':
                case 's':
                case 'S':
                    e.preventDefault();
                    sendMove('D');
                    break;
                case 'ArrowLeft':
                case 'a':
                case 'A':
                    e.preventDefault();
                    sendMove('L');
                    break;
                case 'ArrowRight':
                case 'd':
                case 'D':
                    e.preventDefault();
                    sendMove('R');
                    break;
            }
        });

        // Enter key for joining
        elements.input.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                elements.joinBtn.click();
            }
        });

        // Initialize UI
        updateUI();