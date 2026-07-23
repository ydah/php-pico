--TEST--
JSON encode and decode vector 013
--FILE--
<?php
$value = ['name' => 'item13', 'number' => 170, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item13","number":170,"enabled":false}:item13:170:0
