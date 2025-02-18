#!/bin/bash
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

#arguments: $1 = hostname numeration (optional)
if [ $1 ];
then
    ssh root@spire-server$1 "`env | sed 's/;/\\\\;/g' | sed 's/.*/set &\;/g'` spire-server token generate -spiffeID spiffe://example${1}.org/myagent>/opt/spire/myagentWlC.token"
    cd /mnt/c-spiffe/integration_test && scp root@spire-server$1:/opt/spire/myagentWlC.token .
else
    ssh root@spire-server "`env | sed 's/;/\\\\;/g' | sed 's/.*/set &\;/g'` spire-server token generate -spiffeID spiffe://example.org/myagent>/opt/spire/myagent.token"
    cd /mnt/c-spiffe/integration_test && scp root@spire-server:/opt/spire/myagent.token .
    ssh root@spire-server "`env | sed 's/;/\\\\;/g' | sed 's/.*/set &\;/g'` spire-server token generate -spiffeID spiffe://example.org/myagent>/opt/spire/myagentWlB.token"
    cd /mnt/c-spiffe/integration_test && scp root@spire-server:/opt/spire/myagentWlB.token .
fi
