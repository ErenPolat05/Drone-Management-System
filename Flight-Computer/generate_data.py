import csv
import random

OUTPUT_FILE = "flight_data.csv"

headers = [
    "drone_id",
    "timestamp",
    "latitude",
    "longitude",
    "altitude",
    "pitch",
    "roll",
    "yaw",
    "motor_temp",
    "battery_temp",
    "battery_percent"
]

# Starting location (near Istanbul Kultur University)
latitude = 41.010000
longitude = 28.970000

drone_id = 101
timestamp = 1000

altitude = 0.0
battery_percent = 100.0
motor_temp = 40.0
yaw = 0.0

rows = []

for i in range(1000):

    # ==================================================
    # PHASE 1 (1-200): Normal Takeoff
    # ==================================================
    if i < 200:

        altitude += random.uniform(0.4, 0.8)

        battery_percent -= random.uniform(0.015, 0.025)

        motor_temp += random.uniform(0.03, 0.07)

        pitch = random.uniform(-4, 4)
        roll = random.uniform(-4, 4)

        latitude += random.uniform(-0.00002, 0.00002)
        longitude += random.uniform(-0.00002, 0.00002)

    # ==================================================
    # PHASE 2 (201-400): Cruise Flight
    # ==================================================
    elif i < 400:

        altitude += random.uniform(-0.3, 0.3)

        battery_percent -= random.uniform(0.02, 0.03)

        motor_temp += random.uniform(0.01, 0.04)

        pitch = random.uniform(-5, 5)
        roll = random.uniform(-5, 5)

        latitude += random.uniform(-0.00003, 0.00003)
        longitude += random.uniform(-0.00003, 0.00003)

    # ==================================================
    # PHASE 3 (401-500): Windy Area
    # ==================================================
    elif i < 500:

        altitude += random.uniform(-0.5, 0.5)

        battery_percent -= random.uniform(0.025, 0.035)

        motor_temp += random.uniform(0.02, 0.05)

        pitch = random.uniform(-15, 15)
        roll = random.uniform(-15, 15)

        latitude += random.uniform(-0.00004, 0.00004)
        longitude += random.uniform(-0.00004, 0.00004)

    # ==================================================
    # PHASE 4 (501-650): GPS Drift Simulation
    # ==================================================
    elif i < 650:

        altitude += random.uniform(-0.4, 0.4)

        battery_percent -= random.uniform(0.03, 0.04)

        motor_temp += random.uniform(0.01, 0.04)

        pitch = random.uniform(-6, 6)
        roll = random.uniform(-6, 6)

        latitude += random.uniform(-0.00015, 0.00015)
        longitude += random.uniform(-0.00015, 0.00015)

    # ==================================================
    # PHASE 5 (651-800): Rapid Battery Drain
    # ==================================================
    elif i < 800:

        altitude += random.uniform(-0.5, 0.3)

        battery_percent -= random.uniform(0.07, 0.12)

        motor_temp += random.uniform(0.01, 0.03)

        pitch = random.uniform(-5, 5)
        roll = random.uniform(-5, 5)

        latitude += random.uniform(-0.00004, 0.00004)
        longitude += random.uniform(-0.00004, 0.00004)

    # ==================================================
    # PHASE 6 (801-900): Motor Overheating
    # ==================================================
    elif i < 900:

        altitude += random.uniform(-0.6, 0.3)

        battery_percent -= random.uniform(0.05, 0.08)

        motor_temp += random.uniform(0.25, 0.45)

        pitch = random.uniform(-10, 10)
        roll = random.uniform(-10, 10)

        latitude += random.uniform(-0.00005, 0.00005)
        longitude += random.uniform(-0.00005, 0.00005)

    # ==================================================
    # PHASE 7 (901-1000): Emergency Landing
    # ==================================================
    else:

        remaining_steps = 1000 - i

        altitude -= max(
            altitude / max(remaining_steps, 1),
            1.2
        )

        battery_percent -= random.uniform(0.02, 0.05)

        motor_temp += random.uniform(-0.05, 0.10)

        pitch = random.uniform(-4, 4)
        roll = random.uniform(-4, 4)

        latitude += random.uniform(-0.00003, 0.00003)
        longitude += random.uniform(-0.00003, 0.00003)

    # Keep altitude non-negative
    altitude = max(0, altitude)

    # Simulate heading changes
    yaw = (yaw + random.uniform(-5, 5)) % 360

    # Battery percentage should never increase
    battery_percent = max(5, battery_percent)

    # Motor temperature floor
    motor_temp = max(40, motor_temp)

    # Battery temperature correlated with motor temperature
    battery_temp = (
        28
        + (motor_temp - 40) * 0.45
        + random.uniform(-1.0, 1.0)
    )

    rows.append([
        drone_id,
        timestamp,
        round(latitude, 6),
        round(longitude, 6),
        round(altitude, 2),
        round(pitch, 2),
        round(roll, 2),
        round(yaw, 2),
        round(motor_temp, 2),
        round(battery_temp, 2),
        round(battery_percent, 2)
    ])

    timestamp += 1000

# Ensure the final row represents touchdown
rows[-1][4] = 0.0

with open(
    OUTPUT_FILE,
    "w",
    newline="",
    encoding="utf-8"
) as csv_file:

    writer = csv.writer(csv_file)

    writer.writerow(headers)
    writer.writerows(rows)

print(f"Successfully generated {len(rows)} telemetry records.")
print(f"Output file: {OUTPUT_FILE}")