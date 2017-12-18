# A Proposal for a Universally Unique Identifier Library

## I. Motivation

Universally unique identifiers (*uuid*), also known as Globally Unique Identifiers (*guid*s), are commonly used in many types of applications to uniquely identify data. A standard uuid library would benefit developers that currently have to either use operating system specific APIs for creating new uuids or resort to 3rd party libraries, such as *boost::uuid*.

UUIDs are 128-bit numbers that are for most practical purposes unique, without depending on a central registration authority for ensuring their uniqueness. Although the probability of UUID duplication exists, it is negligible. According to Wikipedia, "*for there to be a one in a billion chance of duplication, 103 trillion version 4 UUIDs must be generated.*"

The library proposed on this paper is a lite one: it enables developers to generate UUIDs based on the operation system facilities, serialize and deserialize UUIDs to and frmo strings, validate UUIDs and other common ensuing operations.

## II. Impact On the Standard

This proposal is a pure library extension. It does not require changes to any standard classes, functions or headers. It does not require any changes in the core language, and it has been implemented in standard C++ as per ISO/IEC 14882:2017.

## III. Design Decisions



## IV. Technical Specifications

## V. Limitations

## VI. References

* [1] Universally unique identifier, https://en.wikipedia.org/wiki/Universally_unique_identifier
* [2] A Universally Unique IDentifier (UUID) URN Namespace, https://tools.ietf.org/html/rfc4122
* [3] Boost Uuid Library http://www.boost.org/doc/libs/1_65_1/libs/uuid/uuid.html
* [4] crossguid - Lightweight cross platform C++ GUID/UUID library, https://github.com/graeme-hill/crossguid
