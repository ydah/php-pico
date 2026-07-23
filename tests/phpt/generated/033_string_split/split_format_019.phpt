--TEST--
string split transform and padding vector 019
--FILE--
<?php
$parts = explode(',', 'v19,v20,v21');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX19'), ':', str_pad($parts[0], 8, '-');
--EXPECT--
v19|v20|v21:3:V20:mix19:v19-----
