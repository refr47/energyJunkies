import asyncio
from huawei_solar import SUN2000Device, create_tcp_client,create_device_instance
from huawei_solar import register_names as rn





async def main():
    dongle_ip = "192.168.178.31"
    port = 502
    unit_id = 1

    print(f"Connecting to huawei {dongle_ip} ....");
    try:
        client = create_tcp_client(host=dongle_ip,port=port,unit_id=unit_id)
        device = await create_device_instance(client)

        print("Querying data ....")
        

        # Wir fügen GRID_ACTIVE_POWER (vom Smart Meter) und ACTIVE_POWER hinzu
        results = await device.batch_update([
            rn.INPUT_POWER,
            rn.ACTIVE_POWER,                    # Aktuelle Ausgangsleistung des Inverters
            rn.POWER_METER_ACTIVE_POWER,               # Smart Meter: Einspeisung (+) / Bezug (-)
            rn.STORAGE_STATE_OF_CAPACITY,
            rn.STORAGE_CHARGE_DISCHARGE_POWER
        ])

        pv_power = results[rn.INPUT_POWER].value
        inverter_power = results[rn.ACTIVE_POWER].value
        grid_power = results[rn.POWER_METER_ACTIVE_POWER].value
        battery_soc = results[rn.STORAGE_STATE_OF_CAPACITY].value
        battery_power = results[rn.STORAGE_CHARGE_DISCHARGE_POWER].value

        # Berechnung des aktuellen Hausverbrauchs (Inverterleistung minus Netzeinspeisung)
        # Wenn grid_power negativ ist (Bezug), wird es durch minus-minus mathematisch addiert.
        house_consumption = inverter_power - grid_power

        print("\n=== Anlagenstatus ===")
        print(f"Aktuelle PV-Leistung: {pv_power} W")
        print(f"Wechselrichter Ausgang: {inverter_power} W")
        
        print("\n=== Smart Meter ===")
        if grid_power > 0:
            print(f"Status: Einspeisung ins Netz: {grid_power} W")
        elif grid_power < 0:
            print(f"Status: Bezug aus dem Netz: {abs(grid_power)} W")
        else:
            print("Status: Netz-Ausgeglichen (0 W)")

        print("\n=== LUNA 2000 status ===")
        print(f"Aktueller Ladezustand (SoC): {battery_soc}%")
        print(f"Batterie Leistung: {battery_power} W")

        print("\n=== Hausverbrauch ===")
        print(f"Aktueller Eigenverbrauch: {house_consumption} W")



        if battery_power > 0:
            print("Status: Akku laden")
        elif battery_power < 0:
            print("stauts: Akku entladen")
        else:
            print("Status: Akku standby")




    except Exception as e:
        print(f"\nException {e} occured.")

if __name__ == "__main__":
    asyncio.run(main())

