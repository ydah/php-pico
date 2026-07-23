--TEST--
string split transform and padding vector 022
--FILE--
<?php
$parts = explode(',', 'v22,v23,v24');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX22'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v22|v23|v24:3:V23:mix22:v22----
