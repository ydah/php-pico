--TEST--
string split transform and padding vector 011
--FILE--
<?php
$parts = explode(',', 'v11,v12,v13');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX11'), ':', str_pad($parts[0], 8, '-');
--EXPECT--
v11|v12|v13:3:V12:mix11:v11-----
