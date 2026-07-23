--TEST--
string split transform and padding vector 021
--FILE--
<?php
$parts = explode(',', 'v21,v22,v23');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX21'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v21|v22|v23:3:V22:mix21:v21---
