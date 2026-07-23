--TEST--
string split transform and padding vector 004
--FILE--
<?php
$parts = explode(',', 'v4,v5,v6');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX4'), ':', str_pad($parts[0], 4, '-');
--EXPECT--
v4|v5|v6:3:V5:mix4:v4--
