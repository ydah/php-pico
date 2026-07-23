--TEST--
string split transform and padding vector 002
--FILE--
<?php
$parts = explode(',', 'v2,v3,v4');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX2'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v2|v3|v4:3:V3:mix2:v2----
