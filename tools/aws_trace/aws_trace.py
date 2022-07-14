import argparse
import json
import logging
import math
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Tuple, Union

import boto3

parser = argparse.ArgumentParser(
    description="Packages all alternative entry points of a binary compiled with the cppless alt-entry option"
)
parser.add_argument(
    "input", metavar="input", type=str, help="The input file to process."
)

args = parser.parse_args()

input_path = Path(args.input)


@dataclass
class TraceSpan:
    name: str
    tags: Dict[str, str]
    start: float
    end: float
    parent: int
    children: List["TraceSpan"]

    def __post_init__(self):
        self.children = []

    def duration_ms(self):
        return (self.end - self.start) / 1000000


leeway = 500

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
    request_id_map = {}
    results = response["results"]
    for result in results:
        for field in result:
            if field["field"] == "@message":
                request_id, report = parse_aws_report(field["value"])
                request_id_map[request_id] = report
    return request_id_map


aws_format_str = "%a, %d %b %Y %H:%M:%S %Z"


@dataclass
class RequestData:
    request_id: str
    start_time: float
    end_time: float


@dataclass
class FunctionData:
    function_name: str
    start: float
    end: float
    request_ids: Dict[str, TraceSpan]
    request_metrics: Dict[str, Dict[str, Union[str, int, float]]]


def process_trace(trace: List[TraceSpan]):
    function_dict: Dict[str, FunctionData] = {}
    for span in trace:
        operation_name = span.name
        tags = span.tags
        if operation_name != "lambda_invocation":
            continue
        request_id = tags["request_id"]

        date = datetime.strptime(tags["response_date"], aws_format_str)
        function_name = tags["function_name"]

        ts = date.replace(tzinfo=timezone.utc).timestamp()
        ts_start = ts - span.duration_ms()
        ts_end = ts + span.duration_ms()
        function_data = function_dict.get(function_name, None)
        if function_data is None:
            function_data = FunctionData(function_name, ts_start, ts_end, {}, {})
            function_dict[function_name] = function_data

        if ts_start < function_data.start:
            function_data.start = ts_start
        if ts_end > function_data.end:
            function_data.end = ts_end

        function_data.request_ids[request_id] = span

    for function_name, function_data in function_dict.items():
        request_id_map = download_metrics(
            function_name, function_data.start - leeway, function_data.end + leeway
        )
        for request_id in function_data.request_ids.keys():
            data = request_id_map.get(request_id)
            if data is None:
                print("No data for request_id: {}".format(request_id))
                continue
            function_data.request_metrics[request_id] = data

    for function_name, function_data in function_dict.items():

        print(f"{function_name}")
        total_mem_gb_seconds = 0
        total_mem_gb_cost = 0
        print(f"  requests:")
        for request_id, metrics in function_data.request_metrics.items():
            span = function_data.request_ids[request_id]
            print(
                f"    {request_id} observed_duration_ms: {span.duration_ms():.0f}, billed_duration_ms: {metrics['billed_duration_ms']}, memory_used: {metrics['memory_used']}"
            )
            total_mem_gb_seconds += metrics["mem_gb_seconds"]
            total_mem_gb_cost += metrics["mb_cost"]
        print(f"  total_mem_gb_seconds: {total_mem_gb_seconds}")
        print(f"  total_mem_gb_cost: {total_mem_gb_cost:.8f}")


def print_span(span: TraceSpan, indent: int):
    print(" " * indent + span.name + f" ({span.duration_ms():.3f})")
    for child in span.children:
        print_span(child, indent + 2)


def fix_time(span: TraceSpan):
    for child in span.children:
        fix_time(child)
    start = span.start
    end = span.end
    for child in span.children:
        if child.start < start or start == 0:
            start = child.start
        if child.end > end or start == 0:
            end = child.end
    span.start = start
    span.end = end


def parse_trace(trace):
    spans: List[TraceSpan] = []
    for span in trace["spans"]:
        spans.append(
            TraceSpan(
                name=span["name"],
                tags=span["tags"],
                start=span["start_time"],
                end=span["end_time"],
                parent=span["parent"],
                children=[],
            )
        )
    roots: List[TraceSpan] = []
    i = 0
    for span in spans:
        x = i
        i += 1
        if span.parent == x:
            roots.append(span)
        else:
            parent = spans[span.parent]
            parent.children.append(span)
    for root in roots:
        fix_time(root)

    return (roots, spans)


with input_path.open("r") as file:
    trace = json.load(file)
(roots, spans) = parse_trace(trace)
process_trace(spans)
