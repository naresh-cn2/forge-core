import csv
with open('huge_data.csv', 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(["id", "name", "city"])
    for i in range(1_000_000):
        writer.writerow([i, f"User_{i}", "Hyderabad"])
