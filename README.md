# Thingsboard C SDK

## Description

This is an SDK for the [ThingsBoard](https://thingsboard.io/) platform. It uses the **HTTP** and **MQTT** APIs.

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)

## Installation

This SDK depends on [cJSON](https://github.com/DaveGamble/cJSON), [CURL](https://github.com/curl/curl) and [mosquitto](https://github.com/eclipse/mosquitto) so before doing anything with the SDK, make sure the previously mentioned packages are installed on your machine.

To build the project `cd/src && make`.

To install `cd/src && sudo make install`.

## Usage

Basic usage is displayed in the **example/example.c** file.

## Configuration

Follow the [ThingsBoard installation guide](https://thingsboard.io/docs/user-guide/install/installation-options/) to configure the ThingsBoard on your machine.

**Side-note: when trying to start the thingsboard service, make sure no other process is using the ports 8080, 1883 and 7070.**
