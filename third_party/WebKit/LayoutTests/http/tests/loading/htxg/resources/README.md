The certificate message files (\*.msg) and the signed exchange files
(\*.htxg) in this directory are generated using the following commands.

gen-certurl and gen-signedexchange are available in [webpackage repository][1].
Revision cf19833 is used to generate these files.

[1]: https://github.com/WICG/webpackage

```
# Install gen-certurl command.
go get github.com/WICG/webpackage/go/signedexchange/cmd/gen-certurl

# Install gen-signedexchange command.
go get github.com/WICG/webpackage/go/signedexchange/cmd/gen-signedexchange

# Generate key/certificate pair for 127.0.0.1 signed-exchanges
(
  cd src/third_party/blink/tools/blinkpy/third_party/wpt/certs;
  openssl ecparam -out 127.0.0.1.sxg.key -name prime256v1 -genkey;
  openssl req -new -sha256 -key 127.0.0.1.sxg.key -out 127.0.0.1.sxg.csr \
    --subj '/CN=127.0.0.1/O=Test/C=US' \
    -config 127.0.0.1.sxg.cnf;
  openssl x509 -req -days 3650 \
    -in 127.0.0.1.sxg.csr -extfile 127.0.0.1.sxg.ext \
    -CA cacert.pem -CAkey cakey.pem -passin pass:web-platform-tests \
    -set_serial 3 -out 127.0.0.1.sxg.pem
)

# Make dummy OCSP data for cbor certificate chains.
echo -n OCSP >/tmp/ocsp

# Generate the certificate chain of "127.0.0.1.sxg.pem".
gen-certurl  \
  -pem ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.pem \
  -ocsp /tmp/ocsp \
  > 127.0.0.1.sxg.pem.cbor

# Generate the signed exchange file.
gen-signedexchange \
  -uri https://www.127.0.0.1/test.html \
  -status 200 \
  -content htxg-location.html \
  -certificate ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.pem \
  -certUrl http://localhost:8000/loading/htxg/resources/127.0.0.1.sxg.pem.cbor \
  -validityUrl https://www.127.0.0.1/loading/htxg/resources/resource.validity.msg \
  -privateKey ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.key \
  -date 2018-04-01T00:00:00Z \
  -expire 168h \
  -o htxg-location.sxg \
  -miRecordSize 100

# Generate the signed exchange file which certificate file is not available.
gen-signedexchange \
  -uri https://www.127.0.0.1/not_found_cert.html \
  -status 200 \
  -content htxg-location.html \
  -certificate ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.pem \
  -certUrl http://localhost:8000/loading/htxg/resources/not_found_cert.pem.cbor \
  -validityUrl https://www.127.0.0.1/loading/htxg/resources/not_found_cert.validity.msg \
  -privateKey ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.key \
  -date 2018-04-01T00:00:00Z \
  -expire 168h \
  -o htxg-cert-not-found.sxg \
  -miRecordSize 100

# Generate the signed exchange file which validity URL is different origin from
# request URL.
gen-signedexchange \
  -uri https://www.127.0.0.1/test.html \
  -status 200 \
  -content htxg-location.html \
  -certificate ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.pem \
  -certUrl http://localhost:8000/loading/htxg/resources/127.0.0.1.sxg.pem.cbor \
  -validityUrl https://www2.127.0.0.1/loading/htxg/resources/resource.validity.msg \
  -privateKey ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.sxg.key \
  -date 2018-04-01T00:00:00Z \
  -expire 168h \
  -o htxg-invalid-validity-url.sxg \
  -miRecordSize 100
```
