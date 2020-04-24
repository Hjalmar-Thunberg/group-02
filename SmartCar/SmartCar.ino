#include <Smartcar.h>
#include <BluetoothSerial.h>
#include <Wire.h>
#include <VL53L0X.h>

BluetoothSerial bluetooth;

// Constansts
const int SPEED = 70;        // 70% of the full speed
const int TURN_ANGLE = 75;   // 75 Degrees to turn
const int MIN_OBSTACLE = 20; // Minimum distance ahead to obstacle
const int GYROSCOPE_OFFSET = 48;
const unsigned int MAX_DISTANCE = 100; // Max distance to measure with ultrasonic

// Unsigned
unsigned int error = 0; //FIXME: Need to calculate error for LIDAR sensor readings.
unsigned int frontDistance;
unsigned int backDistance;

// Boolean
boolean atObstacle = false;
boolean autoDriving = false;

// Ultrasonic trigger pins
const int TRIGGER_PIN = 5; // Trigger signal
const int ECHO_PIN = 18;   // Reads signal

// Sensor pins
SR04 back(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // Ultrasonic
VL53L0X frontSensor;                            // Micro LIDAR
GY50 gyro(GYROSCOPE_OFFSET);                    // Gyroscope

// Odometer
const unsigned long PULSES_PER_METER = 600; // Amount of odometer pulses per 1 meter
DirectionlessOdometer leftOdometer(
    smartcarlib::pins::v2::leftOdometerPin, []() { leftOdometer.update(); }, PULSES_PER_METER);
DirectionlessOdometer rightOdometer(
    smartcarlib::pins::v2::rightOdometerPin, []() { rightOdometer.update(); }, PULSES_PER_METER);

// Car motors
BrushedMotor leftMotor(smartcarlib::pins::v2::leftMotorPins);
BrushedMotor rightMotor(smartcarlib::pins::v2::rightMotorPins);
DifferentialControl control(leftMotor, rightMotor);

// Car initializing
SmartCar car(control, gyro, leftOdometer, rightOdometer);

void setup()
{
    Serial.begin(9600);
    Wire.begin();
    frontSensor.setTimeout(500);
    if (!frontSensor.init())
    {
        Serial.print("Failed to initialize VL53L0X sensor.");
        while (1)
        {
        }
    }
    frontSensor.startContinuous(100);
    bluetooth.begin("Group 2 SmartCar");
    Serial.print("Ready to connect!");
}

void turnRight(unsigned int degrees = TURN_ANGLE, int turnSpeed = car.getSpeed()) // Manual right turn
{
    if (turnSpeed == 0)
        turnSpeed = 10; 
    car.setSpeed(turnSpeed);
    car.setAngle(degrees);
}

void turnLeft(int degrees = -TURN_ANGLE, int turnSpeed = car.getSpeed()) // Manual left turn
{
    if (turnSpeed == 0)
        turnSpeed = 10; //FIXME: Check how much car moves
    car.setSpeed(turnSpeed);
    car.setAngle(degrees);
}

void driveForward(int driveSpeed = SPEED) // Manual forward drive
{
    while (car.getSpeed() > driveSpeed) // Slowly increase carspeed
    {
        car.setSpeed(car.getSpeed() + 10);
        delay(50);
    }
    car.setAngle(0);
    car.setSpeed(driveSpeed);
}

void driveBackward(int driveSpeed = -SPEED) // Manual backwards drive
{
    while (car.getSpeed() > driveSpeed) // Slowly decrease carspeed
    {
        car.setSpeed(car.getSpeed() - 10);
        delay(50);
    }
    car.setAngle(0);
    car.setSpeed(driveSpeed);
}

void brake() // Carstop
{
    car.setSpeed(0);
    car.setAngle(0);
}

void checkDistance() // Obstacle interference
{
    frontDistance = (frontSensor.readRangeSingleMillimeters() / 10) - error; // Divided by 10 to convert to cm
    backDistance = back.getDistance();
    if (frontSensor.timeoutOccurred())
    {
        Serial.print("VL53L0X sensor timeout occurred.");
    }

    if (frontDistance > 0 && frontDistance <= MIN_OBSTACLE)
    {
        atObstacle = true;
        brake();
    }
    if (backDistance > 0 && backDistance <= MIN_OBSTACLE)
    {
        brake();
    }
}

boolean tryTurning() // Try finding a new path around obstacle
{
    atObstacle = false;
    turnRight();     // Turn 75 degrees right
    checkDistance(); // Recheck if there's an obstacle in front, if not return false.
    if (atObstacle)
    {
        turnLeft(); // Turn 150 degrees to the left
        turnLeft();
        checkDistance();
        if (atObstacle)
            car.setAngle(75);
    }
    return atObstacle;
}

void driveWithAvoidance()
{
    while (autoDriving) // While in automatic driving mode.
    {
        driveForward();
        while (!atObstacle) // While you're not at an obstacle
        {
            checkDistance();
        }
        if (atObstacle) // While you're at an obstacle
        {
            driveBackward(-20);
            delay(100); //TODO: Implement odometer distance checking
            brake();
            atObstacle = tryTurning();
        }
    }
}

void handleInput(char input) // Handle serial input if there is any
{
    if (input != 'a')
        autoDriving = false;
    switch (input)
    {
    case 'a': // Automatic driving with obstacle avoidance
        autoDriving = true;
        driveWithAvoidance();
        break;

    case 'l': // Left turn
        turnLeft();
        break;

    case 'r': // Right turn
        turnRight();
        break;

    case 'f': // Forward
        driveForward();
        break;

    case 'b': // Backwards
        driveBackward();
        break;

    case 'i': // Increases carspeed by 10%
        car.setSpeed(car.getSpeed() + 10);
        break;

    case 'd': // Decreases carspeed by 10%
        car.setSpeed(car.getSpeed() - 10);
        break;

    default:
        brake();
    }
}

void readBluetooth()
{
    while (bluetooth.available())
    {
        char input = bluetooth.read();
        handleInput(input);
    }
}

void loop()
{
    readBluetooth();
    checkDistance(); // Checks distance in manual mode.
    gyro.update();
}