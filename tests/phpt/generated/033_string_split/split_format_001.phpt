--TEST--
string split transform and padding vector 001
--FILE--
<?php
$parts = explode(',', 'v1,v2,v3');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX1'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v1|v2|v3:3:V2:mix1:v1---
