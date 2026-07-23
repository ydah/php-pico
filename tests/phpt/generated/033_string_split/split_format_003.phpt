--TEST--
string split transform and padding vector 003
--FILE--
<?php
$parts = explode(',', 'v3,v4,v5');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX3'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v3|v4|v5:3:V4:mix3:v3-----
