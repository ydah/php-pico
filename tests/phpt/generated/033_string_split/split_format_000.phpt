--TEST--
string split transform and padding vector 000
--FILE--
<?php
$parts = explode(',', 'v0,v1,v2');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX0'), ':', str_pad($parts[0], 4, '-');
--EXPECT--
v0|v1|v2:3:V1:mix0:v0--
