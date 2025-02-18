# (C) Copyright 2020-2021 Hewlett Packard Enterprise Development LP
#
# 
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
# 
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# 
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

# Minimum CMake required
cmake_minimum_required(VERSION 3.13)

file(COPY ../tests/resources/ DESTINATION ${PROJECT_BINARY_DIR}/internal/x509util/tests/resources/)

set(SOURCES_CHECK
  check_util.c
  ../util.c
  ../certpool.c
  ../../../utils/util.c
)

add_executable(check_x509util ${SOURCES_CHECK})

target_link_libraries(check_x509util internal ${CHECK_LIBRARIES}
  subunit
  check_pic
  check
  rt
  m
  crypto
  pthread)

add_test(check_x509util check_x509util)

set(SOURCES_CHECK
  check_certpool.c
  ../util.c
  ../certpool.c
  ../../../utils/util.c
)

add_executable(check_certpool ${SOURCES_CHECK})

target_link_libraries(check_certpool internal ${CHECK_LIBRARIES}
  subunit
  check_pic
  check
  rt
  m
  crypto
  pthread)

add_test(check_certpool check_certpool)
