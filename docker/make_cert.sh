#!/bin/sh
 
# Based on http://www.codenes.com/blog/?p=300

echodo()
{
    echo "${@}"
    (${@})
}
 
yearmon()
{
    date '+%Y%m%d'
}
 
C=AU
ST=SA
L=Adelaide
O=codenes
OU=nes
HOST=${1:-`hostname`}
DATE=`yearmon`
CN=example.com
 
csr="TestCertificate.csr"
key="TestCertificate.key"
cert="TestCertificate.crt"
 
# Create the certificate signing request
openssl req -new -passin pass:password -passout pass:password -out $csr <<EOF
${C}
${ST}
${L}
${O}
${OU}
${CN}
$USER@${CN}
.
.
EOF
echo ""
 
[ -f ${csr} ] && echodo openssl req -text -noout -in ${csr}
echo ""
 
# Create the Key
openssl rsa -in privkey.pem -passin pass:password -out ${key}
 
# Create the Certificate
openssl x509 -in ${csr} -out ${cert} -req -signkey ${key} -days 1000
