#define PADDLE_WIDTH_ark 20
#define PADDLE_HEIGHT_ark 4
#define BALL_SIZE_ark 4
#define BRICK_ROWS 4
#define BRICK_COLS 6
#define BRICK_WIDTH 20
#define BRICK_HEIGHT 8
#define BRICK_SPACING 2

// Состояние игры
int paddleX;
int ballX_ark, ballY_ark;
int ballSpeedX_ark, ballSpeedY_ark;
bool bricks[BRICK_ROWS][BRICK_COLS];
int lives = 3;
int score_ark = 0;
bool gameRunning = false;