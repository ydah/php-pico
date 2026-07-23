--TEST--
string split transform and padding vector 018
--FILE--
<?php
$parts = explode(',', 'v18,v19,v20');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX18'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v18|v19|v20:3:V19:mix18:v18----
