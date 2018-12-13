#include "swiot_ca.h"

static const char *swiot_ca_crt = \
{
    \
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIICxjCCAa6gAwIBAgIJAJk1DbZBu8FDMA0GCSqGSIb3DQEBCwUAMBMxETAPBgNV\r\n" \
    "BAMMCE15VGVzdENBMB4XDTE3MTEwMjEzNDI0N1oXDTE5MTEwMjEzNDI0N1owEzER\r\n" \
    "MA8GA1UEAwwITXlUZXN0Q0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB\r\n" \
    "AQDshDho6ef1JClDJ24peSsXdFnFO3xIB7+BSp1YPcOvmRECKUG0mLORw3hNm15m\r\n" \
    "8eGOn1iLGE/xKlaZ74/xjyq8f7qIGZCmvZj59m+eiJCAmy8SiUJZtSVoOlOzepJd\r\n" \
    "PoDgcBvDKA4ogZ3iJHMUNI3EdlD6nrKEJF2qe2JUrL0gv65uo2/N7XVNvE87Dk3J\r\n" \
    "83KyCAmeu+x+moS1ILnjs2DuPEGSxZqzf7IQMbXuNWJYAOZg9t4Fg0YjTiAaWw3G\r\n" \
    "JKAoMY4tI3JCqlvwGR4lH7kfk3WsD4ofGlFhxU4nEG0xgnJl8BcoJWD1A2RjGe1f\r\n" \
    "qCijqPSe93l2wt8OpbyHzwc7AgMBAAGjHTAbMAwGA1UdEwQFMAMBAf8wCwYDVR0P\r\n" \
    "BAQDAgEGMA0GCSqGSIb3DQEBCwUAA4IBAQAi+t5jBrMxFzoF76kyRd3riNDlWp0w\r\n" \
    "NCewkohBkwBHsQfHzSnc6c504jdyzkEiD42UcI8asPsJcsYrQ+Uo6OBn049u49Wn\r\n" \
    "zcSERVSVec1/TAPS/egFTU9QMWtPSAm8AEaQ6YYAuiwOLCcC+Cm/a3e3dWSRWt8o\r\n" \
    "LqKX6CWTlmKWe182MhFPpZYxZQLGapti4R4mb5QusUbc6tXbkcX82GjDPTOuAw7b\r\n" \
    "mWpzVd5xnlp7Vz+50u+YaAYUmCobg0hR/AuTrA4GDMlgzTnuZQhF6o8iVkypXOtS\r\n" \
    "Ufz6X3tVVErVVc7UUfzSnupHj1M2h4rzlQ3oqHoAEnXcJmV4f/Pf/6FW\r\n" \
    "-----END CERTIFICATE-----"
};

const char *swiot_ca_get(void)
{
    return swiot_ca_crt;
}
