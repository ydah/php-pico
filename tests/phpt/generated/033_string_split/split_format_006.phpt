--TEST--
string split transform and padding vector 006
--FILE--
<?php
$parts = explode(',', 'v6,v7,v8');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX6'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v6|v7|v8:3:V7:mix6:v6----
