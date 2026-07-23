--TEST--
JSON encode and decode vector 009
--FILE--
<?php
$value = ['name' => 'item09', 'number' => 118, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item09","number":118,"enabled":false}:item09:118:0
