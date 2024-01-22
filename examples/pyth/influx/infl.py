import influxdb_client, os, time
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS


#token = "INFLUXDB_TOKEN=_5U_MGYn_02TzdeNYxEkDr4EjjcEHhOs3YmxeebJy8dVvEgsiatOOwLR6MvXuVUk79v4cpgQCyaFsA4iv2KUZA=="
token = os.environ.get("INFLUXDB_TOKEN")
bucket = "testPython"
org = "home"
# Store the URL of your InfluxDB instance
url="http://localhost:8086"

client = influxdb_client.InfluxDBClient(
    url=url,
    token=token,
    org=org
)

# Write script
write_api = client.write_api(write_options=SYNCHRONOUS,ssl=False)

for value in range(45):
  point = (
    Point("m1")
    .tag("tagname1", "test")
    .field("field1", value+20)
	 .field("field2",value+100)
	 .field("f3",value-10)
	 .field("f4",value*2)
  )
  write_api.write(bucket=bucket, org="home", record=point)
  time.sleep(1) # separate points by 1 second

