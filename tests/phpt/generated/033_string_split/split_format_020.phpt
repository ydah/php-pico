--TEST--
string split transform and padding vector 020
--FILE--
<?php
$parts = explode(',', 'v20,v21,v22');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX20'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v20|v21|v22:3:V21:mix20:v20--
