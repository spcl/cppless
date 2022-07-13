from datetime import datetime, timezone
import json
import logging
import math
from typing import Dict, Tuple, Union
import boto3
import time

leeway = 10

logs_client = boto3.client(
    service_name="logs",
)

mem_gb_second_price = 0.0000166667
eph_gb_second_price = 0.0000000367


def parse_aws_report(log: str) -> Tuple[str, Dict[str, Union[str, int, float]]]:
    aws_vals = {}
    for line in log.split("\t"):
        if not line.isspace():
            split = line.split(":")
            aws_vals[split[0]] = split[1].split()[0]
    if "START RequestId" in aws_vals:
        request_id = aws_vals["START RequestId"]
    else:
        request_id = aws_vals["REPORT RequestId"]

    output: Dict[str, Union[str, int, float]] = dict()
    output["duration"] = int(float(aws_vals["Duration"]) * 1000)
    output["memory_used"] = float(aws_vals["Max Memory Used"])
    if "Init Duration" in aws_vals:
        output["init_duration"] = int(float(aws_vals["Init Duration"]) * 1000)
    output["billed_duration_ms"] = int(aws_vals["Billed Duration"])
    output["memory_mb"] = int(aws_vals["Memory Size"])

    gb_seconds = (
        int(aws_vals["Billed Duration"]) / 1000  # Billed duration is in ms
    ) * (
        int(aws_vals["Memory Size"]) / 1024  # Memory size is in MB
    )
    output["mem_gb_seconds"] = gb_seconds
    output["mb_cost"] = gb_seconds * mem_gb_second_price
    return (request_id, output)


def download_metrics(
    function_name: str,
    start_time: float,
    end_time: float,
):

    query = logs_client.start_query(
        logGroupName="/aws/lambda/{}".format(function_name),
        queryString="filter @message like /REPORT/",
        startTime=math.floor(start_time),
        endTime=math.ceil(end_time + 1),
        limit=10_000,
    )
    query_id = query["queryId"]
    response = None

    while response is None or response["status"] == "Running":
        logging.info("Waiting for AWS query to complete ...")
        time.sleep(0.2)
        response = logs_client.get_query_results(queryId=query_id)
    # results contain a list of matches
    # each match has multiple parts, we look at `@message` since this one
    # contains the report of invocation
    results = response["results"]
    for result in results:
        for field in result:
            if field["field"] == "@message":
                request_id, report = parse_aws_report(field["value"])
        print(request_id, report)


aws_format_str = "%a, %d %b %Y %H:%M:%S %Z"


def process_trace(trace):
    start = None
    end = None
    function_names = {}
    for span in trace["spans"]:
        operation_name = span["name"]
        tags = span["tags"]
        if operation_name != "lambda_invocation":
            continue
        request_id = tags["request_id"]

        date = datetime.strptime(tags["response_date"], aws_format_str)
        ts = date.replace(tzinfo=timezone.utc).timestamp()
        if start is None or ts < start:
            start = ts
        if end is None or ts > end:
            end = ts


with open("./output/trace.json") as file:
    trace = json.load(file)
process_trace(trace)

download_metrics("echo", start - leeway, end + leeway)
