--TEST--
nested array copy-on-write vector 000
--FILE--
<?php
$tree = ['node' => ['value' => 1, 'items' => [2]]];
$copy = $tree;
$copy['node']['value'] += 2;
$copy['node']['items'][] = 5;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
1:3:2:2,5
