/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file aws_iot_certifcates.c
 * @brief File to store the AWS certificates in the form of arrays
 */

#ifdef __cplusplus
extern "C" {
#endif

const char aws_root_ca_pem[] = {"-----BEGIN CERTIFICATE-----\n\
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n\
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n\
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n\
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n\
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n\
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n\
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n\
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n\
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n\
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n\
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n\
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n\
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n\
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n\
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n\
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n\
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n\
rqXRfboQnoZsG4q5WTP468SQvvG5\n\
-----END CERTIFICATE-----\n"};

const char certificate_pem_crt[] = {"-----BEGIN CERTIFICATE-----\n\
MIIDWTCCAkGgAwIBAgIUEjc1ouNc2gsXg2w2Ema3aBTtW4owDQYJKoZIhvcNAQEL\n\
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n\
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTIzMTAxNTA4NDMx\n\
NloXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n\
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMYupn3+X0Z48rXtr7pD\n\
PejvZ9YX0tgxgCQ0+t3l9PX9nEyPajg29xXaGypG/bfqjFlH6UVa6SJIi1FLPuU+\n\
Co5fiTz64G7aOclfp1aIJES35bnWrsIJnca3p9SpFfKIx0KMd2phJDq3+SDUgfCc\n\
2DrBA8xA6aRtLgCZ3fyNdhrCHaHtlLtnfztU8PtIxXcNzLxWKcSVT+mldq8ta2L5\n\
W1wP/UncpqI8kIGFB0Tdo5Adbk839PBRIt6D9dLoyM0Nzf+1f861QNm5MXcKIu/H\n\
bRpL3CpcZsC3+GcfMoEt0fHUsSQNURf0CtnHDnysQXFz1ZBfHMrojXQ1/3LKWe12\n\
vCUCAwEAAaNgMF4wHwYDVR0jBBgwFoAUQKFD+6+0VT7I52LiW/NU1mYvnOIwHQYD\n\
VR0OBBYEFEOaj/jN2q4kbK/Voar9YL1OvNFLMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n\
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQCx0MmenC5nHT2w62JK0DPe2NBT\n\
jSW1o8zAuvj40UxSre/50PS+9HVP4J+1K6Dc6KDGmpLoIi4REDeN/Ahy3DUISNVv\n\
mckxyqtUdb+RaOpBhqZ4VI0snHxm1w7XqVOnfNYpxTga8Y7b6J6yCTZrXdqFdid4\n\
qA7O9FwHGOibmQw/05SZ4BS1YtPKwRHhqLODjKe9vUOKAa/s+9kut9zqZeRX+Jso\n\
pz8vSdQfQDUVrnz6LZG9psKv+tmQMnmx2WrIb+OkywifGe9Rv1qj8VjtKJ67YQWx\n\
VnN9EY8M+hOqnUCM9auzhlY/LsIKY1+iwZZvadLnNEPGXhdOe4zG6IcNeITU\n\
-----END CERTIFICATE-----\n"};

const char private_pem_key[] = {"-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpAIBAAKCAQEAxi6mff5fRnjyte2vukM96O9n1hfS2DGAJDT63eX09f2cTI9q\n\
ODb3FdobKkb9t+qMWUfpRVrpIkiLUUs+5T4Kjl+JPPrgbto5yV+nVogkRLfludau\n\
wgmdxren1KkV8ojHQox3amEkOrf5INSB8JzYOsEDzEDppG0uAJnd/I12GsIdoe2U\n\
u2d/O1Tw+0jFdw3MvFYpxJVP6aV2ry1rYvlbXA/9SdymojyQgYUHRN2jkB1uTzf0\n\
8FEi3oP10ujIzQ3N/7V/zrVA2bkxdwoi78dtGkvcKlxmwLf4Zx8ygS3R8dSxJA1R\n\
F/QK2ccOfKxBcXPVkF8cyuiNdDX/cspZ7Xa8JQIDAQABAoIBAB8Nnj7txcYIG5fQ\n\
J5HbCW9fW/jbAMmpb/e8aXatmqWU2JHSgwVN74d4BnuoZgcUdGtk7jNU3GTn0xSo\n\
4rEO+J54b8ujT9luzlE0cMcJTA2Mdy2YaweydAt4KY8T84FuVzKhSfDcSc39l8bC\n\
JZ4rJWKHbe3qWfHOvb+QXI0/KTtl2otCAujxpudDla09R6cJWqaAVRoMuUOeUKT0\n\
n7itg+/tVgT23BJQdPtrydXAAMwGrqaJpGa06Ba8ArLUsxmUGxUsJfjqYLyH0ghf\n\
maOwGZfhk0jtJMZRaAY+07RGb9H2+r6bvTh74fyLHD2DsAikWOV9zm0xUey8jCvL\n\
u3iSOB0CgYEA7OgRhWEy9/pgabXhRRRdP8bxXEvlw/zJeTrB15qwBjyJ2fIJ7IUV\n\
T7HklBoG60CENsZzwx35qGB7KINDg8IEwvhNZxqt4yNSOfxsobRczFOiA5JwNAfm\n\
ki22QirnfbChK8e4bMMKThjgH4+T0WPDy0Ur2VlOl8+DirVDbKWj3vsCgYEA1iec\n\
IwXUgQxkUDTaLe+j61HoPYUBVQWssUOG6G9h32U+pFH+INGUQxUpG0C7FaVX/BjZ\n\
3BsNl8S07b+LMf139mFlYCY4vCW5GFTBs7T6kDkvVr+mQPHznmx+dELGw1SAaRN5\n\
0Q5CstiOw28HfzotWMzqUnOG/D+L96LUgCHQZ18CgYANA3Jy8TPri0VgiS0mrkex\n\
CSyY9VJZwbkPf7rGLSkeLpUj87e31lelWRBFUmiu44xftecGAM+GAEbDovJCepXo\n\
X9tgnoaOw5HwvXz6JP4z+yQbLiAbu5Ne1EP+vnyY2ur/jKkE0HHweE1XbnugNOq8\n\
b8BI4C8BB9Uh+XMKtM6boQKBgQCc2+h2krejSnNTeFLy4Jt9KsJkBT8DiqOCN4bi\n\
S2sx2RO4AkKUwU3KkP5J8vDVmwVGJuG0YbBkIPWxSuJm3FR5B7/cXEKkNTFNTA+R\n\
VJdO+kPdU4Uv/sjFmoxJFfxGaGeiD1zjroFWRF7VcUbZfsBmUbm0aOBPdTBNU1w4\n\
+guPGwKBgQCYHqHQaQkFMX0aWiO+adTN1hRTPUPga0dF6EM1H6gpNefaHwksqq2S\n\
AoNHdSU9YA8Qeat9jx2rlI1ZWRRbSEIXhzoAktY7iDpxUd8ZIEr7kU5nE+2rLouH\n\
OZH8s0JAMo1x7dlf3PftSZL/Ib6QSvDmiItwiDcxWHSqXNPXaud+6Q==\n\
-----END RSA PRIVATE KEY-----\n"};


#ifdef __cplusplus
}
#endif
