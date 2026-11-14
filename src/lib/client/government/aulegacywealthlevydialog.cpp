#include "aulegacywealthlevydialog.h"
#include "auapp.h"
#include "auglobal.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

AuLegacyWealthLevyDialog::AuLegacyWealthLevyDialog(QWidget* parent) : AuDialog(parent)
{
	setAuDlgTitle(tr("Levy Wealth Tax"));
	setAuDlgIcon(QIcon(":data/CommGovernment.png"));
	// AuConfiscate, executable 0x14016be50/0x14016c8a0. Keep its warning
	// and public-statement requirement visible beside the original controls.
	auto* description = new QLabel(tr("<html>Taxation is confiscation of the private property of your citizens. "
		"Confiscation of private wealth has dire consequences.<br>"
		"&bull; Players become angry.<br>"
		"&bull; Loyalty of all cities, with or without banks, is reduced by 2% for every 1% of tax levied.<br>"
		"&bull; Cities whose loyalty goes below zero will revolt instead of surrendering wealth.<br><br>"
		"Wealth is confiscated from private accounts.<br>"
		"&bull; Empire cities with a bank surrender citizen account wealth, if they do not revolt.<br>"
		"&bull; Empire players, on and off line, surrender personal account wealth if they are in an empire city with a bank.<br>"
		"&bull; Empire spacecraft, private and subsidized, surrender ship account wealth if they are in an empire city with a bank.</html>"), this);
	description->setWordWrap(true);
	auto* percent = new QSpinBox(this); percent->setRange(1, 100); percent->setAlignment(Qt::AlignRight);
	percent->setToolTip(tr("Enter the percentage of tax to levy on private wealth."));
	auto* warning = new QLabel(tr("% WARNING! REVOLT WILL OCCUR!"));
	warning->setStyleSheet("color: red;");
	warning->setWordWrap(false);
	connect(percent, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [warning](int percentage) {
		warning->setVisible(percentage >= 50);
	});
	warning->setVisible(percent->value() >= 50);
	auto* statement = new QLineEdit(this); statement->setMaxLength(200); statement->setMinimumWidth(400);
	statement->setToolTip(tr("Public Statement: explain to the citizens why this executive order was necessary."));
	auto* suggest = new QPushButton(tr("Suggest Public Statement"), this);
	auto* avatars = new QCheckBox(tr("Avatars Excluded"), this);
	auto* government = new QCheckBox(tr("Members of Government Excluded"), this);
	auto* privateShips = new QCheckBox(tr("Private Spacecraft Excluded"), this);
	auto* subsidized = new QCheckBox(tr("Subsidized Spacecraft Excluded"), this);
	auto* levy = new QPushButton(tr("Levy Wealth Tax"), this);
	levy->setAutoDefault(true); levy->setDefault(true); levy->setEnabled(false);
	connect(statement, &QLineEdit::textChanged, this, [levy](const QString& text) { levy->setEnabled(!text.isEmpty()); });
	connect(levy, &QPushButton::clicked, this, [this, percent, statement, avatars, government, privateShips, subsidized] {
		if(statement->text().trimmed().isEmpty()) return;
		const quint8 exemptions = (avatars->isChecked() ? 1 : 0) | (government->isChecked() ? 2 : 0)
			| (privateShips->isChecked() ? 4 : 0) | (subsidized->isChecked() ? 8 : 0);
		auApp->RequestLegacyWealthLevy(quint8(percent->value()), statement->text(), exemptions);
		accept();
	});
	connect(suggest, &QPushButton::clicked, this, [statement] {
		// Complete original 195-entry SuggestStatement selection, 0x14016ca60.
		const QStringList slogans = {
			tr("Acid, Amnesty, and Abortion for All"),
			tr("A Chicken in Every Pot. Two cars in every garage."),
			tr("All the way with LBJ"),
			tr("A time for greatness 1960"),
			tr("Are You Better Off Than You Were Four Years Ago?"),
			tr("Be a hero"),
			tr("Change is"),
			tr("Change We Can Believe In"),
			tr("Country First"),
			tr("Defeat the New Deal and Its Reckless Spending"),
			tr("Don't swap horses in midstream"),
			tr("Drill, baby, Drill!"),
			tr("Four more years of the full dinner pail"),
			tr("For People, for a Change"),
			tr("He kept us out of war"),
			tr("Who but Hoover?"),
			tr("Hope and Change"),
			tr("I like Ike"),
			tr("I propose a New Deal"),
			tr("I still like Ike"),
			tr("I'm just wild about Harry"),
			tr("In Your Heart, You Know He's Right"),
			tr("In Your Guts, You Know He's Nuts"),
			tr("It's the economy, stupid"),
			tr("It's Time to Change America"),
			tr("Keep Cool and Keep Coolidge"),
			tr("Let Well Enough Alone"),
			tr("Let's Get Another Deck"),
			tr("Let's Make It a Landon-Slide"),
			tr("Life, Liberty, and Landon"),
			tr("Ma, Ma where's my Pa?"),
			tr("Morning Again in America"),
			tr("No Fourth Term Either"),
			tr("Peace and Prosperity"),
			tr("Pour it on 'em, Harry!"),
			tr("Putting People First"),
			tr("Reform, prosperity and peace"),
			tr("Remember Hoover!"),
			tr("Restore America Now"),
			tr("Return to normalcy"),
			tr("Roosevelt for Ex-President"),
			tr("Ross for Boss"),
			tr("Si Se Puede!"),
			tr("Rum, Romanism and Rebellion"),
			tr("There are two Americas"),
			tr("There's No Indispensable Man"),
			tr("Tilden or Blood!"),
			tr("Tippecanoe and Tyler too"),
			tr("To Begin Anew..."),
			tr("Turn the Rascals Out"),
			tr("Vote as You Shot"),
			tr("We are turning the corner"),
			tr("We Polked you in '44, We shall Pierce you in '52"),
			tr("We Want Willkie"),
			tr("Win with Willkie"),
			tr("Yes We Can"),
			tr("Abolish the wages system"),
			tr("All power to the Soviets"),
			tr("All Power to the Imagination!"),
			tr("Arbeit Macht Frei"),
			tr("Anything But Umno"),
			tr("Are you thinking what we're thinking?"),
			tr("Britain Deserves Better"),
			tr("Behindertsein ist schoen"),
			tr("Better dead than Red"),
			tr("Better war than Pact"),
			tr("Bigger cages! Longer chains!"),
			tr("Black is beautiful"),
			tr("Black Power"),
			tr("Bread and roses"),
			tr("Catch up and overtake America!"),
			tr("Come and take it"),
			tr("Deeds Not Words"),
			tr("Democracia Real Ya!"),
			tr("Deus, Patria, e Familia"),
			tr("Doctors need to be preserved, not reserved."),
			tr("Don't let him take Britain back to the 1980s"),
			tr("Don't Stop, Keep Going On!"),
			tr("Each for all and all for each"),
			tr("Eat the Rich"),
			tr("Ein Volk, ein Reich, ein Fuhrer"),
			tr("England Will Fight to the Last American"),
			tr("Every Man a King"),
			tr("Fifty-Four Forty or Fight"),
			tr("Fuhrer befiehl, wir folgen dir!"),
			tr("God made Adam and Eve, not Adam and Steve"),
			tr("Got Guv"),
			tr("Go For Growth"),
			tr("Had enough?"),
			tr("Hasta la Victoria Siempre"),
			tr("Heim ins Reich"),
			tr("He's Good Enough for Me"),
			tr("Hey, Hey, LBJ, how many kids you kill today?"),
			tr("I agree with Nick"),
			tr("In Your Heart You Know He's Right"),
			tr("It's Scotland's oil"),
			tr("It's Time"),
			tr("Jedem das Seine"),
			tr("Labour is not Working"),
			tr("Liberte, Egalite, Fraternite"),
			tr("Lips That Touch Liquor Must Never Touch Mine"),
			tr("Maggie, Maggie, Maggie - Out, Out, Out"),
			tr("Make love not war"),
			tr("Me ne frego!"),
			tr("Never had it so good"),
			tr("Never been had so good"),
			tr("New Labour, New Danger"),
			tr("Ni dieu, Ni maitre"),
			tr("No Surrender!"),
			tr("Not a step back!"),
			tr("Nothing About Us Without Us!"),
			tr("Now... The Next Steps"),
			tr("No War but Class War"),
			tr("Patria o Muerte"),
			tr("Peron o muerte"),
			tr("Piss On Pity"),
			tr("Power to the people"),
			tr("Rally Around O'Malley"),
			tr("Remember Pearl Harbor"),
			tr("Remember the Alamo"),
			tr("Remember the Maine"),
			tr("Revolution is not a dinner party"),
			tr("Safety First"),
			tr("Save the Bay"),
			tr("Serve the People"),
			tr("Simon Go Back"),
			tr("Stanley Baldwin the Man You Can Trust!"),
			tr("Stay the course"),
			tr("The Buck Stops Here"),
			tr("The rich get richer and the poor get poorer"),
			tr("They shall not pass"),
			tr("Three Word Chant!"),
			tr("This will not stand!"),
			tr("Think globally, act locally"),
			tr("Trust Baldwin he will steer you to safety!"),
			tr("Diversity is always better."),
			tr("Tyler and Texas!"),
			tr("Una, Grande y Libre!"),
			tr("Venceremos"),
			tr("Vote for Change"),
			tr("We are the 99%"),
			tr("We are the 1%"),
			tr("Wir sind das Volk"),
			tr("Workers of the world, unite!"),
			tr("This is a White Man's Government!"),
			tr("Grant beat Davis - Greeley bailed him"),
			tr("Grant us another term"),
			tr("Grandfather's hat fits Ben"),
			tr("Full Dinner Pail"),
			tr("Hoover we trusted, now we're busted."),
			tr("Sunflowers die in November"),
			tr("Give 'Em Hell, Harry!"),
			tr("Go clean for Gene"),
			tr("Bozo and the Pineapple"),
			tr("Let's make America great again"),
			tr("Read my lips"),
			tr("Yes, America Can!"),
			tr("The Strength and Experience to Bring Real Change"),
			tr("I'm In. Are You?"),
			tr("Believe in America"),
			tr("Liberty: Too big to fail"),
			tr("True integrity, not false promises"),
			tr("Real hope. Real change."),
			tr("Tune out the media."),
			tr("What can Ron Paul un-do for you?"),
			tr("Dr. Ron Paul cured my apathy"),
			tr("Ron Paul - His word is gold"),
			tr("Ron the Incorruptible"),
			tr("Who owns your candidate?"),
			tr("Dr. Ron Paul earned our trust"),
			tr("Straight talk, not double talk"),
			tr("We're inspired, not hired!"),
			tr("Your life, your way"),
			tr("'1984' was a warning, not a blueprint"),
			tr("Principled positions, not partisan politics"),
			tr("Legalize Freedom"),
			tr("No strings attached"),
			tr("Liberty and Justice for All!"),
			tr("We won't get fooled again"),
			tr("Give peace a chance"),
			tr("End bi-partisan corruption"),
			tr("End the Fed"),
			tr("Freedom is popular"),
			tr("Just say 'No' to the status quo"),
			tr("Freedom: A sustainable development"),
			tr("We the People"),
			tr("Make your kids proud"),
			tr("Take the red pill"),
			tr("The Real Deal"),
			tr("Try honesty for a 'change'"),
			tr("It just doesn't get any better."),
			tr("Change you don't have to hope for"),
			tr("Principle over politics"),
			tr("Answer freedom's call."),
			tr("No new taxes."),
		};
		statement->setText(slogans.at(g_Au.Dice().Roll(1, slogans.size(), -1)));
	});
	auto* layout = new QGridLayout;
	layout->addWidget(description, 0, 0, 1, 3);
	layout->addWidget(suggest, 1, 0);
	layout->addWidget(statement, 1, 1, 1, 2);
	layout->addWidget(percent, 2, 1); layout->addWidget(new QLabel(QStringLiteral("%"), this), 2, 2);
	layout->addWidget(warning, 2, 2);
	layout->addWidget(avatars, 3, 1, 1, 2); layout->addWidget(government, 4, 1, 1, 2);
	layout->addWidget(privateShips, 5, 1, 1, 2); layout->addWidget(subsidized, 6, 1, 1, 2);
	layout->addWidget(levy, 7, 1, 1, 2); layout->setRowStretch(8, 1);
	layout->setMargin(2); layout->setSpacing(2); setAuDlgLayout(layout);
}
