--TEST--
string split transform and padding vector 014
--FILE--
<?php
$parts = explode(',', 'v14,v15,v16');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX14'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v14|v15|v16:3:V15:mix14:v14----
