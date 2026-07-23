--TEST--
associative key order vector 028
--FILE--
<?php
$items = ['left' => 38, 4 => 104, 'right' => 170];
unset($items[4]);
$items['tail'] = 171;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:38,170,171:1
