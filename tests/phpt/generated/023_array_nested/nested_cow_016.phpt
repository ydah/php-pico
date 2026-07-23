--TEST--
nested array copy-on-write vector 016
--FILE--
<?php
$tree = ['node' => ['value' => 65, 'items' => [66]]];
$copy = $tree;
$copy['node']['value'] += 4;
$copy['node']['items'][] = 71;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
65:69:66:66,71
