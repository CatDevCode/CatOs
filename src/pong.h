// ================ PONG ================
#define PADDLE_HEIGHT 12
#define PADDLE_WIDTH 3
#define BALL_SIZE 3
#define MAX_BALL_SPEED_X 3   // Максимальная горизонтальная скорость
#define MAX_BALL_SPEED_Y 2   // Максимальная вертикальная скорость
int player1Y = 32;      // Позиция левой ракетки
int player2Y = 32;      // Позиция правой ракетки
int ballX = 64;         // Позиция мяча
int ballY = 32;
int ballSpeedX = 1;     // Скорость мяча
int ballSpeedY = 1;
int score1 = 0;         // Счет игроков
int score2 = 0;