--TEST--
nested array copy-on-write vector 005
--FILE--
<?php
$tree = ['node' => ['value' => 21, 'items' => [22]]];
$copy = $tree;
$copy['node']['value'] += 7;
$copy['node']['items'][] = 30;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
21:28:22:22,30
