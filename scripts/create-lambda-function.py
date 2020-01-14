#!/usr/bin/env python3

import os
import sys
import zipfile
import shutil
import argparse
import hashlib
import base64
import boto3

def create_function_package(output, binary):
    PACKAGE_FILES = {
        "binary": binary,
        "main.py": "lambda-main.py",
    }

    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as funczip:
        for fn, fp in PACKAGE_FILES.items():
            funczip.write(fp, fn)

def install_lambda_package(package_file, function_name, role, region, delete=False):
    with open(package_file, 'rb') as pfin:
        package_data = pfin.read()

    client = boto3.client('lambda', region_name=region)

    if delete:
        try:
            client.delete_function(FunctionName=function_name)
            print("Deleted function '{}'.".format(function_name))
        except:
            pass

    response = client.create_function(
        FunctionName=function_name,
        Runtime='python3.6',
        Role=role,
        Handler='main.handler',
        Code={
            'ZipFile': package_data
        },
        Timeout=60,
        MemorySize=3008
    )

    print("Created function '{}' ({}).".format(function_name, response['FunctionArn']))

def main():
    parser = argparse.ArgumentParser(description="Generate and install Lambda functions.")
    parser.add_argument('--delete', dest='delete', action='store_true', default=False)
    parser.add_argument('--role', dest='role', action='store')
    parser.add_argument('--region', dest='region', action='store')
    parser.add_argument('--name', dest='name', action='store')
    parser.add_argument('--binary', dest='binary')

    args = parser.parse_args()

    if not args.binary:
        raise Exception("Cannot find binary")

    if not args.role:
        raise Exception("Please provide function role.")

    function_name = args.name
    function_file = "{}.zip".format(function_name)
    try:
        create_function_package(function_file, args.binary)
        print("Installing lambda function {}... ".format(function_name), end='')
        install_lambda_package(function_file, function_name, args.role, args.region,
                               delete=args.delete)
    finally:
        os.remove(function_file)

if __name__ == '__main__':
    main()
