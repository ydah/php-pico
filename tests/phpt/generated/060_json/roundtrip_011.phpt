--TEST--
JSON encode and decode vector 011
--FILE--
<?php
$value = ['name' => 'item11', 'number' => 144, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item11","number":144,"enabled":false}:item11:144:0
