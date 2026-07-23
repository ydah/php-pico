--TEST--
string split transform and padding vector 005
--FILE--
<?php
$parts = explode(',', 'v5,v6,v7');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX5'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v5|v6|v7:3:V6:mix5:v5---
